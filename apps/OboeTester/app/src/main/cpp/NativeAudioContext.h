/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef NATIVEOBOE_NATIVEAUDIOCONTEXT_H
#define NATIVEOBOE_NATIVEAUDIOCONTEXT_H

#include <dlfcn.h>
#include <thread>
#include <vector>

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"

#include "AudioStreamGateway.h"
#include "ImpulseGenerator.h"
#include "InputStreamCallbackAnalyzer.h"
#include "ManyToMultiConverter.h"
#include "MonoToMultiConverter.h"
#include "MultiChannelRecording.h"
#include "OboeStreamCallbackProxy.h"
#include "PlayRecordingCallback.h"
#include "SawPingGenerator.h"
#include "SineGenerator.h"

#define MAX_SINE_OSCILLATORS     8
#define AMPLITUDE_SINE           1.0
#define FREQUENCY_SAW_PING       800.0
#define AMPLITUDE_SAW_PING       1.0
#define AMPLITUDE_IMPULSE        0.7

#define NANOS_PER_MICROSECOND    ((int64_t) 1000)
#define NANOS_PER_MILLISECOND    (1000 * NANOS_PER_MICROSECOND)
#define NANOS_PER_SECOND         (1000 * NANOS_PER_MILLISECOND)

#define LIB_AAUDIO_NAME          "libaaudio.so"
#define FUNCTION_IS_MMAP         "AAudioStream_isMMapUsed"

/**
 * Implement the native API for the Oboe Tester.
 * Manage a stream.
 * Generate signals, etc.
 */
class NativeAudioContext {
public:

    NativeAudioContext();

    void close();

    bool isMMapUsed();

    int open(jint sampleRate,
             jint channelCount,
             jint format,
             jint sharingMode,
             jint performanceMode,
             jint deviceId,
             jint sessionId,
             jint framesPerBurst, jboolean isInput);

    void setToneType(int toneType) {
        LOGI("%s(%d)", __func__, toneType);
        mToneType = (ToneType) toneType;
        connectTone();
    }

    int32_t getFramesPerBlock() {
        return (callbackSize == 0) ? mFramesPerBurst : callbackSize;
    }

    void printScheduler() {
        if (audioStreamGateway != nullptr) {
            int scheduler = audioStreamGateway->getScheduler();
            LOGI("scheduler = 0x%08x, SCHED_FIFO = 0x%08X\n", scheduler, SCHED_FIFO);
        }
    }


    oboe::Result pause() {
        LOGD("NativeAudioContext::%s() called", __func__);
        oboe::Result result = oboe::Result::OK;
        stopBlockingIOThread();
        if (oboeStream != nullptr) {
            result = oboeStream->requestPause();
            printScheduler();
        }
        return result;
    }

    oboe::Result stopAudio() {
        LOGD("NativeAudioContext::%s() called", __func__);
        oboe::Result result = oboe::Result::OK;
        stopBlockingIOThread();

        if (oboeStream != nullptr) {
            result = oboeStream->requestStop();
            printScheduler();
        }
        return result;
    }

    oboe::Result stopPlayback() {
        LOGD("NativeAudioContext::%s() called", __func__);
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

    oboe::Result stop() {
        oboe::Result result1 = stopPlayback();
        oboe::Result result2 = stopAudio();

        for (int i = 0; i < mChannelCount; i++) {
            sineGenerators[i].stop();
        }
        impulseGenerator.stop();
        sawPingGenerator.stop();
        if (audioStreamGateway != nullptr) {
            audioStreamGateway->stop();
        }
        return (result1 != oboe::Result::OK) ? result1 : result2;
    }

    oboe::Result startPlayback() {
        stop();
        LOGD("NativeAudioContext::%s() called", __func__);
        oboe::AudioStreamBuilder builder;
        builder.setChannelCount(mChannelCount)
                ->setSampleRate(mSampleRate)
                ->setFormat(oboe::AudioFormat::Float)
                ->setCallback(&mPlayRecordingCallback)
                ->setAudioApi(oboe::AudioApi::OpenSLES);
        oboe::Result result = builder.openStream(&playbackStream);
        LOGD("NativeAudioContext::startPlayback() openStream() returned %d", result);
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

    static void threadCallback(NativeAudioContext *context) {
        LOGD("%s: called", __func__);
        context->runBlockingIO();
        LOGD("%s: exiting", __func__);
    }

    oboe::Result start() {
        stop();

        for (int i = 0; i < mChannelCount; i++) {
            sineGenerators[i].start();
        }
        impulseGenerator.start();
        sawPingGenerator.start();
        if (audioStreamGateway != nullptr) {
            audioStreamGateway->start();
        }

        LOGD("OboeAudioStream_start: start called");
        oboe::Result result = oboe::Result::OK;
        if (oboeStream != nullptr) {
            result = oboeStream->requestStart();
        }

        if (!useCallback && result == oboe::Result::OK) {
            LOGD("OboeAudioStream_start: start thread for blocking I/O");
            // Instead of using the callback, start a thread that reads or writes the stream.
            threadEnabled.store(true);
            dataThread = new std::thread(threadCallback, this);
        }
        LOGD("OboeAudioStream_start: start returning %d", result);
        return result;
    }

    void setAudioApi(oboe::AudioApi audioApi) {
        mAudioApi = audioApi;
    }

    void setToneEnabled(bool enabled) {
        LOGD("%s(%d)", __func__, enabled ? 1 : 0);
        // sineGenerator.setEnabled(enabled); // not needed
        sawPingGenerator.setEnabled(enabled);
        // impulseGenerator.setEnabled(enabled); // not needed
    }

    void setAmplitude(double amplitude) {
        LOGD("%s(%f)", __func__, amplitude);
        for (int i = 0; i < mChannelCount; i++) {
            sineGenerators[i].amplitude.setValue(amplitude);
        }
        sawPingGenerator.amplitude.setValue(amplitude);
        impulseGenerator.amplitude.setValue(amplitude);
    }

    void setCallbackReturnStop(bool b) {
        oboeCallbackProxy.setCallbackReturnStop(b);
    }

    int64_t getCallbackCount() {
        return oboeCallbackProxy.getCallbackCount();
    }

    void setChannelEnabled(int channelIndex, bool enabled);

    oboe::AudioStream           *oboeStream = nullptr;
    InputStreamCallbackAnalyzer  mInputAnalyzer;
    bool                         useCallback = true;
    bool                         callbackReturnStop = false;
    int                          callbackSize = 0;
    bool                         mIsMMapUsed = false;

private:

    // WARNING - must match order in strings.xml and OboeAudioOutputStream.java
    enum ToneType {
        SawPing = 0,
        Sine = 1,
        Impulse = 2
    };

    void connectTone();

    void runBlockingIO();

    void stopBlockingIOThread() {
        if (!useCallback) {
            // stop a thread that runs in place of the callback
            threadEnabled.store(false); // ask thread to exit its loop
            if (dataThread != nullptr) {
                dataThread->join();
                dataThread = nullptr;
            }
        }

    }

    oboe::AudioApi               mAudioApi = oboe::AudioApi::Unspecified;
    int32_t                      mFramesPerBurst = 0;
    int32_t                      mChannelCount = 0;
    int32_t                      mSampleRate = 0;
    ToneType                     mToneType = ToneType::Sine;

    std::atomic<bool>            threadEnabled{false};
    std::thread                 *dataThread = nullptr;

    OboeStreamCallbackProxy      oboeCallbackProxy;
    std::vector<SineGenerator>   sineGenerators;

    ImpulseGenerator             impulseGenerator;
    SawPingGenerator             sawPingGenerator;
    oboe::AudioStream           *playbackStream = nullptr;

    std::unique_ptr<float []>               dataBuffer{};
    std::unique_ptr<ManyToMultiConverter>   manyToMulti;
    std::unique_ptr<MonoToMultiConverter>   monoToMulti;
    std::unique_ptr<AudioStreamGateway>     audioStreamGateway{};
    std::unique_ptr<MultiChannelRecording>  mRecording{};

    PlayRecordingCallback        mPlayRecordingCallback;

    bool                       (*mAAudioStream_isMMap)(AAudioStream *stream) = nullptr;
    void                        *mLibHandle = nullptr;

};

#endif //NATIVEOBOE_NATIVEAUDIOCONTEXT_H
