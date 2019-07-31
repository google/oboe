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

#include <fstream>
#include <iostream>
#include <vector>

#include "util/WaveFileWriter.h"

#include "NativeAudioContext.h"

static oboe::AudioApi convertNativeApiToAudioApi(int nativeApi) {
    switch (nativeApi) {
        default:
        case NATIVE_MODE_UNSPECIFIED:
            return oboe::AudioApi::Unspecified;
        case NATIVE_MODE_AAUDIO:
            return oboe::AudioApi::AAudio;
        case NATIVE_MODE_OPENSLES:
            return oboe::AudioApi::OpenSLES;
    }
}

class MyOboeOutputStream : public WaveFileOutputStream {
public:
    void write(uint8_t b) override {
        mData.push_back(b);
    }

    int32_t length() {
        return (int32_t) mData.size();
    }

    uint8_t *getData() {
        return mData.data();
    }

private:
    std::vector<uint8_t> mData;
};

bool ActivityContext::useCallback = true;
int  ActivityContext::callbackSize = 0;

oboe::AudioStream * ActivityContext::getOutputStream() {
    for (auto entry : mOboeStreams) {
        oboe::AudioStream *oboeStream = entry.second;
        if (oboeStream->getDirection() == oboe::Direction::Output) {
            return oboeStream;
        }
    }
    return nullptr;
}

oboe::AudioStream * ActivityContext::getInputStream() {
    for (auto entry : mOboeStreams) {
        oboe::AudioStream *oboeStream = entry.second;
        if (oboeStream != nullptr) {
            if (oboeStream->getDirection() == oboe::Direction::Input) {
                return oboeStream;
            }
        }
    }
    return nullptr;
}

void ActivityContext::freeStreamIndex(int32_t streamIndex) {
    mOboeStreams.erase(streamIndex);
}

int32_t ActivityContext::allocateStreamIndex() {
    return mNextStreamHandle++;
}

void ActivityContext::close(int32_t streamIndex) {
    stopBlockingIOThread();

    LOGD("%s() delete stream %d ", __func__, streamIndex);
    oboe::AudioStream *oboeStream = getStream(streamIndex);
    if (oboeStream != nullptr) {
        oboeStream->close();
        delete oboeStream;
        freeStreamIndex(streamIndex);
    }
}

bool ActivityContext::isMMapUsed(int32_t streamIndex) {
    oboe::AudioStream *oboeStream = getStream(streamIndex);
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

oboe::Result ActivityContext::pause() {
    LOGD("NativeAudioContext::%s() called", __func__);
    oboe::Result result = oboe::Result::OK;
    stopBlockingIOThread();
    for (auto entry : mOboeStreams) {
        oboe::AudioStream *oboeStream = entry.second;
        result = oboeStream->requestPause();
        printScheduler();
    }
    return result;
}

oboe::Result ActivityContext::stopAllStreams() {
    oboe::Result result = oboe::Result::OK;
    stopBlockingIOThread();
    LOGD("ActivityContext::stopAllStreams() called");
    for (auto entry : mOboeStreams) {
        LOGD("ActivityContext::stopAllStreams() handle = %d, stream %p",
             entry.first, entry.second);
        oboe::AudioStream *oboeStream = entry.second;
        result = oboeStream->requestStop();
        printScheduler();
    }
    return result;
}


void ActivityContext::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    // We needed the proxy because we did not know the channelCount when we setup the Builder.
    if (useCallback) {
        LOGD("ActivityContext::open() set callback to use oboeCallbackProxy, size = %d",
             callbackSize);
        builder.setCallback(&oboeCallbackProxy);
        builder.setFramesPerCallback(callbackSize);
    }
}

int ActivityContext::open(
        jint nativeApi,
        jint sampleRate,
        jint channelCount,
        jint format,
        jint sharingMode,
        jint performanceMode,
        jint deviceId,
        jint sessionId,
        jint framesPerBurst,
        jboolean isInput) {

    oboe::AudioApi audioApi = oboe::AudioApi::Unspecified;
    switch (nativeApi) {
        case NATIVE_MODE_UNSPECIFIED:
        case NATIVE_MODE_AAUDIO:
        case NATIVE_MODE_OPENSLES:
            audioApi = convertNativeApiToAudioApi(nativeApi);
            break;
        default:
            return (jint) oboe::Result::ErrorOutOfRange;
    }

    int32_t streamIndex = allocateStreamIndex();
    if (streamIndex < 0) {
        LOGE("ActivityContext::open() stream array full");
        return (jint) oboe::Result::ErrorNoFreeHandles;
    }

    if (channelCount < 0 || channelCount > 256) {
        LOGE("ActivityContext::open() channels out of range");
        return (jint) oboe::Result::ErrorOutOfRange;
    }

    // Create an audio output stream.
    LOGD("ActivityContext::open() try to create OboeStream #%d", streamIndex);
    oboe::AudioStreamBuilder builder;
    builder.setChannelCount(channelCount)
            ->setDirection(isInput ? oboe::Direction::Input : oboe::Direction::Output)
            ->setSharingMode((oboe::SharingMode) sharingMode)
            ->setPerformanceMode((oboe::PerformanceMode) performanceMode)
            ->setDeviceId(deviceId)
            ->setSessionId((oboe::SessionId) sessionId)
            ->setSampleRate(sampleRate)
            ->setFormat((oboe::AudioFormat) format);

    configureBuilder(isInput, builder);

    if (audioApi == oboe::AudioApi::OpenSLES) {
        builder.setFramesPerCallback(framesPerBurst);
    }
    builder.setAudioApi(audioApi);

    // Open a stream based on the builder settings.
    oboe::AudioStream *oboeStream = nullptr;
    oboe::Result result = builder.openStream(&oboeStream);
    LOGD("ActivityContext::open() builder.openStream() returned %d", result);
    if (result != oboe::Result::OK) {
        delete oboeStream;
        oboeStream = nullptr;
        freeStreamIndex(streamIndex);
        streamIndex = -1;
    } else {
        mOboeStreams[streamIndex] = oboeStream;

        mChannelCount = oboeStream->getChannelCount(); // FIXME store per stream
        mFramesPerBurst = oboeStream->getFramesPerBurst();
        mSampleRate = oboeStream->getSampleRate();

        createRecording();

        finishOpen(isInput, oboeStream);
    }

    if (!useCallback) {
        int numSamples = getFramesPerBlock() * mChannelCount;
        dataBuffer = std::make_unique<float[]>(numSamples);
    }


    return ((int)result < 0) ? (int)result : streamIndex;
}

oboe::Result ActivityContext::start() {
    LOGD("ActivityContext: %s() called", __func__);
    oboe::Result result = oboe::Result::OK;
    oboe::AudioStream *inputStream = getInputStream();
    oboe::AudioStream *outputStream = getOutputStream();
    if (inputStream == nullptr && outputStream == nullptr) {
        LOGD("%s() - no streams defined", __func__);
        return oboe::Result::ErrorInvalidState; // not open
    }

    stop();

    configureForStart();

    result = startStreams();

    if (!useCallback && result == oboe::Result::OK) {
        LOGD("start thread for blocking I/O");
        // Instead of using the callback, start a thread that writes the stream.
        threadEnabled.store(true);
        dataThread = new std::thread(threadCallback, this);
    }

    return result;
}

int32_t  ActivityContext::saveWaveFile(const char *filename) {
    if (mRecording == nullptr) {
        LOGW("ActivityContext::saveWaveFile(%s) but no recording!", filename);
        return -1;
    }
    if (mRecording->getSizeInFrames() == 0) {
        LOGW("ActivityContext::saveWaveFile(%s) but no frames!", filename);
        return -2;
    }
    MyOboeOutputStream outStream;
    WaveFileWriter writer(&outStream);

    writer.setFrameRate(mSampleRate);
    writer.setSamplesPerFrame(mRecording->getChannelCount());
    writer.setBitsPerSample(24);
    float buffer[mRecording->getChannelCount()];
    // Read samples from start to finish.
    mRecording->rewind();
    for (int32_t frameIndex = 0; frameIndex < mRecording->getSizeInFrames(); frameIndex++) {
        mRecording->read(buffer, 1 /* numFrames */);
        for (int32_t i = 0; i < mRecording->getChannelCount(); i++) {
            writer.write(buffer[i]);
        }
    }
    writer.close();

    if (outStream.length() > 0) {
        auto myfile = std::ofstream(filename, std::ios::out | std::ios::binary);
        myfile.write((char *) outStream.getData(), outStream.length());
        myfile.close();
    }

    return outStream.length();
}

// =================================================================== ActivityTestOutput
void ActivityTestOutput::close(int32_t streamIndex) {
    ActivityContext::close(streamIndex);
    manyToMulti.reset(nullptr);
    monoToMulti.reset(nullptr);
    mSinkFloat.reset();
    mSinkI16.reset();
}

void ActivityTestOutput::setChannelEnabled(int channelIndex, bool enabled) {
    if (manyToMulti == nullptr) {
        return;
    }
    if (enabled) {
        switch (mToneType) {
            case ToneType::Sine:
                sineOscillators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            case ToneType::Sawtooth:
                sawtoothOscillators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            default:
                break;
        }
    } else {
        manyToMulti->inputs[channelIndex]->disconnect();
    }
}

void ActivityTestOutput::configureForStart() {
    manyToMulti = std::make_unique<ManyToMultiConverter>(mChannelCount);

    mSinkFloat = std::make_unique<SinkFloat>(mChannelCount);
    mSinkI16 = std::make_unique<SinkI16>(mChannelCount);

    oboe::AudioStream *outputStream = getOutputStream();
    {
        double frequency = 660.0;
        for (int i = 0; i < mChannelCount; i++) {
            sineOscillators[i].setSampleRate(outputStream->getSampleRate());
            sineOscillators[i].frequency.setValue(frequency);
            frequency *= 4.0 / 3.0; // each sine is at a higher frequency
            sineOscillators[i].amplitude.setValue(AMPLITUDE_SINE);
            sineOscillators[i].output.connect(manyToMulti->inputs[i].get());
        }
    }

    manyToMulti->output.connect(&(mSinkFloat.get()->input));
    manyToMulti->output.connect(&(mSinkI16.get()->input));

    configureStreamGateway();
}

void ActivityTestOutput::configureStreamGateway() {
    oboe::AudioStream *outputStream = getOutputStream();
    if (outputStream->getFormat() == oboe::AudioFormat::I16) {
        audioStreamGateway.setAudioSink(mSinkI16);
    } else if (outputStream->getFormat() == oboe::AudioFormat::Float) {
        audioStreamGateway.setAudioSink(mSinkFloat);
    }

    if (useCallback) {
        oboeCallbackProxy.setCallback(&audioStreamGateway);
    }

    // Set starting size of buffer.
    constexpr int kDefaultNumBursts = 2; // "double buffer"
    int32_t numBursts = kDefaultNumBursts;
    // callbackSize is used for both callbacks and blocking write
    numBursts = (callbackSize <= mFramesPerBurst)
                ? kDefaultNumBursts
                : ((callbackSize * kDefaultNumBursts) + mFramesPerBurst - 1)
                  / mFramesPerBurst;
    outputStream->setBufferSizeInFrames(numBursts * mFramesPerBurst);

}

void ActivityTestOutput::runBlockingIO() {
    int32_t framesPerBlock = getFramesPerBlock();
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;

    oboe::AudioStream *oboeStream = getOutputStream();
    if (oboeStream == nullptr) {
        LOGE("%s() : no stream found\n", __func__);
        return;
    }

    while (threadEnabled.load()
           && callbackResult == oboe::DataCallbackResult::Continue) {
        // generate output by calling the callback
        callbackResult = audioStreamGateway.onAudioReady(oboeStream,
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

// ======================================================================= ActivityTestInput
void ActivityTestInput::configureForStart() {
    mInputAnalyzer.reset();
    if (useCallback) {
        oboeCallbackProxy.setCallback(&mInputAnalyzer);
    }
    mInputAnalyzer.setRecording(mRecording.get());
}

void ActivityTestInput::runBlockingIO() {
    int32_t framesPerBlock = getFramesPerBlock();
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;

    oboe::AudioStream *oboeStream = getInputStream();
    if (oboeStream == nullptr) {
        LOGE("%s() : no stream found\n", __func__);
        return;
    }

    while (threadEnabled.load()
           && callbackResult == oboe::DataCallbackResult::Continue) {
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
    }
}

oboe::Result ActivityRecording::stopPlayback() {
    LOGD("ActivityRecording::%s() called", __func__);
    oboe::Result result = oboe::Result::OK;
    if (playbackStream != nullptr) {
        result = playbackStream->requestStop();
        playbackStream->close();
        mPlayRecordingCallback.setRecording(nullptr);
        delete playbackStream;
        playbackStream = nullptr;
    }
    return result;
}

oboe::Result ActivityRecording::startPlayback() {
    stop();
    LOGD("ActivityRecording::%s() called", __func__);
    oboe::AudioStreamBuilder builder;
    builder.setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setFormat(oboe::AudioFormat::Float)
            ->setCallback(&mPlayRecordingCallback)
            ->setAudioApi(oboe::AudioApi::OpenSLES);
    oboe::Result result = builder.openStream(&playbackStream);
    LOGD("ActivityRecording::%s() openStream() returned %d", __func__, result);
    if (result != oboe::Result::OK) {
        delete playbackStream;
        playbackStream = nullptr;
    } else if (playbackStream != nullptr) {
        if (mRecording != nullptr) {
            mRecording->rewind();
            mPlayRecordingCallback.setRecording(mRecording.get());
            result = playbackStream->requestStart();
        }
    }
    return result;
}

// ======================================================================= ActivityTapToTone
void ActivityTapToTone::configureForStart() {
    monoToMulti = std::make_unique<MonoToMultiConverter>(mChannelCount);

    mSinkFloat = std::make_unique<SinkFloat>(mChannelCount);
    mSinkI16 = std::make_unique<SinkI16>(mChannelCount);

    oboe::AudioStream *outputStream = getOutputStream();
    sawPingGenerator.setSampleRate(outputStream->getSampleRate());
    sawPingGenerator.frequency.setValue(FREQUENCY_SAW_PING);
    sawPingGenerator.amplitude.setValue(AMPLITUDE_SAW_PING);

    sawPingGenerator.output.connect(&(monoToMulti->input));
    monoToMulti->output.connect(&(mSinkFloat.get()->input));
    monoToMulti->output.connect(&(mSinkI16.get()->input));

    sawPingGenerator.setEnabled(false);
    configureStreamGateway();
}

// ======================================================================= ActivityRoundTripLatency
void ActivityFullDuplex::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    if (isInput) {
        // Ideally the output streams should be opened first.
        oboe::AudioStream *outputStream = getOutputStream();
        if (outputStream != nullptr) {
            // Make sure the capacity is bigger than two bursts.
            int32_t burst = outputStream->getFramesPerBurst();
            builder.setBufferCapacityInFrames(2 * burst);
        }
    }
}

// ======================================================================= ActivityEcho
void ActivityEcho::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexEcho.get() == nullptr) {
        mFullDuplexEcho = std::make_unique<FullDuplexEcho>();
    }
    // only output uses a callback, input is polled
    if (!isInput) {
        builder.setCallback(mFullDuplexEcho.get());
    }
}

void ActivityEcho::finishOpen(bool isInput, oboe::AudioStream *oboeStream) {
    if (isInput) {
        mFullDuplexEcho->setInputStream(oboeStream);
    } else {
        mFullDuplexEcho->setOutputStream(oboeStream);
    }
}

// ======================================================================= ActivityRoundTripLatency
void ActivityRoundTripLatency::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexLatency.get() == nullptr) {
        mFullDuplexLatency = std::make_unique<FullDuplexLatency>();
    }
    if (!isInput) {
        // only output uses a callback, input is polled
        builder.setCallback(mFullDuplexLatency.get());
    }
}

void ActivityRoundTripLatency::finishOpen(bool isInput, oboe::AudioStream *oboeStream) {
    if (isInput) {
        mFullDuplexLatency->setInputStream(oboeStream);
        mFullDuplexLatency->setRecording(mRecording.get());
    } else {
        mFullDuplexLatency->setOutputStream(oboeStream);
    }
}

// ======================================================================= ActivityGlitches
void ActivityGlitches::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexGlitches.get() == nullptr) {
        mFullDuplexGlitches = std::make_unique<FullDuplexGlitches>();
    }
    if (!isInput) {
        // only output uses a callback, input is polled
        builder.setCallback(mFullDuplexGlitches.get());
    }
}

void ActivityGlitches::finishOpen(bool isInput, oboe::AudioStream *oboeStream) {
    if (isInput) {
        mFullDuplexGlitches->setInputStream(oboeStream);
        mFullDuplexGlitches->setRecording(mRecording.get());
    } else {
        mFullDuplexGlitches->setOutputStream(oboeStream);
    }
}
