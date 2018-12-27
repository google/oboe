/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NativeAudioContext.h"

#define SECONDS_TO_RECORD   10


NativeAudioContext::NativeAudioContext()
    : sineGenerators(MAX_SINE_OSCILLATORS) {
}

void NativeAudioContext::close() {
    stopBlockingIOThread();

    if (oboeStream != nullptr) {
        oboeStream->close();
    }
    delete oboeStream;
    oboeStream = nullptr;
    manyToMulti.reset(nullptr);
    monoToMulti.reset(nullptr);
    audioStreamGateway.reset(nullptr);
}

bool NativeAudioContext::isMMapUsed() {
    if (oboeStream != nullptr && oboeStream->usesAAudio()) {
        if (mAAudioStream_isMMap == nullptr) {
            mLibHandle = dlopen(LIB_AAUDIO_NAME, 0);
            if (mLibHandle == nullptr) {
                LOGI("%s() could not find " LIB_AAUDIO_NAME, __func__);
                return false;
            }

            mAAudioStream_isMMap = (bool (*)(AAudioStream *stream))
                    dlsym(mLibHandle, FUNCTION_IS_MMAP);

            if(mAAudioStream_isMMap == nullptr) {
                LOGI("%s() could not find " FUNCTION_IS_MMAP, __func__);
                return false;
            }
        }
        AAudioStream *aaudioStream = (AAudioStream *) oboeStream->getUnderlyingStream();
        return mAAudioStream_isMMap(aaudioStream);
    }
    return false;
}

void NativeAudioContext::connectTone() {
    if (monoToMulti != nullptr) {
        LOGI("%s() mToneType = %d", __func__, mToneType);
        switch (mToneType) {
            case ToneType::SawPing:
                sawPingGenerator.output.connect(&(monoToMulti->input));
                monoToMulti->output.connect(&(audioStreamGateway.get()->input));
                break;
            case ToneType::Sine:
                for (int i = 0; i < mChannelCount; i++) {
                    sineGenerators[i].output.connect(manyToMulti->inputs[i].get());
                }
                manyToMulti->output.connect(&(audioStreamGateway.get()->input));
                break;
            case ToneType::Impulse:
                impulseGenerator.output.connect(&(monoToMulti->input));
                monoToMulti->output.connect(&(audioStreamGateway.get()->input));
                break;
        }
    }
}

void NativeAudioContext::setChannelEnabled(int channelIndex, bool enabled) {
    if (manyToMulti == nullptr) {
        return;
    }
    if (enabled) {
        sineGenerators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
    } else {
        manyToMulti->inputs[channelIndex]->disconnect();
    }
}

int NativeAudioContext::open(jint sampleRate,
         jint channelCount,
         jint format,
         jint sharingMode,
         jint performanceMode,
         jint deviceId,
         jint sessionId,
         jint framesPerBurst, jboolean isInput) {
    if (oboeStream != NULL) {
        return (jint) oboe::Result::ErrorInvalidState;
    }
    if (channelCount < 0 || channelCount > 256) {
        LOGE("NativeAudioContext::open() channels out of range");
        return (jint) oboe::Result::ErrorOutOfRange;
    }

    // Create an audio output stream.
    LOGD("NativeAudioContext::open() try to create the OboeStream");
    oboe::AudioStreamBuilder builder;
    builder.setChannelCount(channelCount)
            ->setDirection(isInput ? oboe::Direction::Input : oboe::Direction::Output)
            ->setSharingMode((oboe::SharingMode) sharingMode)
            ->setPerformanceMode((oboe::PerformanceMode) performanceMode)
            ->setDeviceId(deviceId)
            ->setSessionId((oboe::SessionId) sessionId)
            ->setSampleRate(sampleRate)
            ->setFormat((oboe::AudioFormat) format);

    // We needed the proxy because we did not know the channelCount when we setup the Builder.
    if (useCallback) {
        builder.setCallback(&oboeCallbackProxy);
        builder.setFramesPerCallback(callbackSize);
    }

    if (mAudioApi == oboe::AudioApi::OpenSLES) {
        builder.setFramesPerCallback(framesPerBurst);
    }
    builder.setAudioApi(mAudioApi);

    // Open a stream based on the builder settings.
    oboe::Result result = builder.openStream(&oboeStream);
    LOGD("NativeAudioContext::open() open(b) returned %d", result);
    if (result != oboe::Result::OK) {
        delete oboeStream;
        oboeStream = nullptr;
    } else {
        mChannelCount = oboeStream->getChannelCount();
        mFramesPerBurst = oboeStream->getFramesPerBurst();
        mSampleRate = oboeStream->getSampleRate();
        if (isInput) {
            mInputAnalyzer.reset();
            if (useCallback) {
                oboeCallbackProxy.setCallback(&mInputAnalyzer);
            }
            mRecording = std::make_unique<MultiChannelRecording>(mChannelCount,
                                                                 SECONDS_TO_RECORD * mSampleRate);
            mInputAnalyzer.setRecording(mRecording.get());
        } else {
            double frequency = 440.0;
            for (int i = 0; i < mChannelCount; i++) {
                sineGenerators[i].setSampleRate(oboeStream->getSampleRate());
                sineGenerators[i].frequency.setValue(frequency);
                frequency *= 4.0 / 3.0; // each sine is a higher frequency
                sineGenerators[i].amplitude.setValue(AMPLITUDE_SINE);
            }

            impulseGenerator.setSampleRate(oboeStream->getSampleRate());
            impulseGenerator.frequency.setValue(440.0);
            impulseGenerator.amplitude.setValue(AMPLITUDE_IMPULSE);

            sawPingGenerator.setSampleRate(oboeStream->getSampleRate());
            sawPingGenerator.frequency.setValue(FREQUENCY_SAW_PING);
            sawPingGenerator.amplitude.setValue(AMPLITUDE_SAW_PING);

            manyToMulti = std::make_unique<ManyToMultiConverter>(mChannelCount);
            monoToMulti = std::make_unique<MonoToMultiConverter>(mChannelCount);

            // We needed the proxy because we did not know the channelCount
            // when we setup the Builder.
            audioStreamGateway = std::make_unique<AudioStreamGateway>(mChannelCount);

            connectTone();

            if (useCallback) {
                oboeCallbackProxy.setCallback(audioStreamGateway.get());
            }

            // Set starting size of buffer.
            constexpr int kDefaultNumBursts = 2; // "double buffer"
            int32_t numBursts = kDefaultNumBursts;
            // callbackSize is used for both callbacks and blocking write
            numBursts = (callbackSize <= mFramesPerBurst)
                        ? kDefaultNumBursts
                        : ((callbackSize * kDefaultNumBursts) + mFramesPerBurst - 1)
                          / mFramesPerBurst;
            oboeStream->setBufferSizeInFrames(numBursts * mFramesPerBurst);
        }

        if (!useCallback) {
            int numSamples = getFramesPerBlock() * mChannelCount;
            dataBuffer = std::make_unique<float []>(numSamples);
        }

        mIsMMapUsed = isMMapUsed();
    }

    return (int) result;
}

void NativeAudioContext::runBlockingIO() {
    int32_t framesPerBlock = getFramesPerBlock();
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;
    while (threadEnabled.load()
           && callbackResult == oboe::DataCallbackResult::Continue) {
        if (oboeStream->getDirection() == oboe::Direction::Input) {
            // read from input
            auto result = oboeStream->read(dataBuffer.get(),
                                           framesPerBlock,
                                           NANOS_PER_SECOND);
            if (!result) {
                LOGE("%s() : read() returned %s\n", __func__, convertToText(result.error()));
                break;
            }
            int32_t framesRead = result.value();
            if (framesRead < framesPerBlock) { // timeout?
                LOGE("%s() : read() read %d of %d\n", __func__, framesRead, framesPerBlock);
                break;
            }

            // analyze input
            callbackResult = mInputAnalyzer.onAudioReady(oboeStream,
                                                         dataBuffer.get(),
                                                         framesRead);
        } else if (audioStreamGateway != nullptr) {  // OUTPUT?
            // generate output by calling the callback
            callbackResult = audioStreamGateway->onAudioReady(oboeStream,
                                                              dataBuffer.get(),
                                                              framesPerBlock);

            auto result = oboeStream->write(dataBuffer.get(),
                                            framesPerBlock,
                                            NANOS_PER_SECOND);

            if (!result) {
                LOGE("%s() returned %s\n", __func__, convertToText(result.error()));
                break;
            }
            int32_t framesWritten = result.value();
            if (framesWritten < framesPerBlock) {
                LOGE("%s() : write() wrote %d of %d\n", __func__, framesWritten, framesPerBlock);
                break;
            }
        }
    }

}
