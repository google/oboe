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
#include <jni.h>
#include <thread>
#include <vector>

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"

#include "AudioStreamGateway.h"
#include "flowgraph/ImpulseOscillator.h"
#include "flowgraph/ManyToMultiConverter.h"
#include "flowgraph/MonoToMultiConverter.h"
#include "flowgraph/SinkFloat.h"
#include "flowgraph/SinkI16.h"
#include "flowgraph/SineOscillator.h"
#include "flowgraph/SawtoothOscillator.h"
#include "InputStreamCallbackAnalyzer.h"
#include "MultiChannelRecording.h"
#include "OboeStreamCallbackProxy.h"
#include "PlayRecordingCallback.h"
#include "SawPingGenerator.h"

// These must match order in strings.xml and in StreamConfiguration.java
#define NATIVE_MODE_UNSPECIFIED  0
#define NATIVE_MODE_OPENSLES     1
#define NATIVE_MODE_AAUDIO       2

#define MAX_SINE_OSCILLATORS     8
#define AMPLITUDE_SINE           1.0
#define AMPLITUDE_SAWTOOTH       0.5
#define FREQUENCY_SAW_PING       800.0
#define AMPLITUDE_SAW_PING       0.8
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

    void close(int32_t streamIndex);

    bool isMMapUsed(int32_t streamIndex);

    oboe::AudioStream *getStream(int32_t streamIndex) {
        return mOboeStreams[streamIndex]; // TODO range check
    }

    int open(jint nativeApi,
             jint sampleRate,
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
        for (int32_t i = 0; i < kMaxStreams; i++) {
            oboe::AudioStream *oboeStream = mOboeStreams[i];
            if (oboeStream != nullptr) {
                result = oboeStream->requestPause();
                printScheduler();
            }
        }
        return result;
    }

    oboe::Result stopAudio() {
        LOGD("NativeAudioContext::%s() called", __func__);
        oboe::Result result = oboe::Result::OK;
        stopBlockingIOThread();

        for (int32_t i = 0; i < kMaxStreams; i++) {
            oboe::AudioStream *oboeStream = mOboeStreams[i];
            if (oboeStream != nullptr) {
                result = oboeStream->requestStop();
                printScheduler();
            }
        }
        LOGD("NativeAudioContext::%s() returns %d", __func__, result);
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
        oboe::Result resultStopPlayback = stopPlayback();
        oboe::Result resultStopAudio = stopAudio();

        LOGD("NativeAudioContext::%s() stop modules", __func__);
        for (int i = 0; i < mChannelCount; i++) {
            sineOscillators[i].stop();
            sawtoothOscillators[i].stop();
        }
        impulseGenerator.stop();
        sawPingGenerator.stop();
        if (mSinkFloat) {
            mSinkFloat->stop();
        }
        if (mSinkI16) {
            mSinkI16->stop();
        }

        oboe::Result result = (resultStopPlayback != oboe::Result::OK)
                ? resultStopPlayback
                : resultStopAudio;

        LOGD("NativeAudioContext::%s() returns %d", __func__, result);
        return result;
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

        LOGD("NativeAudioContext: %s() called", __func__);
        configureForActivityType();

        bool gotOne = false;
        for (int32_t i = 0; i < kMaxStreams; i++) {
            gotOne = (mOboeStreams[i] != nullptr);
            if (gotOne) break;
        }
        if (!gotOne) {
            LOGD("NativeAudioContext: %s() did not find a stream", __func__);
            return oboe::Result::ErrorNull;
        }

        stop();

        LOGD("NativeAudioContext: %s() start modules", __func__);
        for (int i = 0; i < mChannelCount; i++) {
            sineOscillators[i].start();
            sawtoothOscillators[i].start();
        }
        impulseGenerator.start();
        sawPingGenerator.start();
        if (mSinkFloat) {
            mSinkFloat->start();
        }
        if (mSinkI16) {
            mSinkI16->start();
        }

        LOGD("NativeAudioContext: %s start stream", __func__);
        oboe::Result result = oboe::Result::OK;
        for (int32_t i = 0; i < kMaxStreams; i++) {
            oboe::AudioStream *oboeStream = mOboeStreams[i];
            if (oboeStream != nullptr) {
                result = oboeStream->requestStart();

                if (!useCallback && result == oboe::Result::OK) {
                    LOGD("OboeAudioStream_start: start thread for blocking I/O");
                    // Instead of using the callback, start a thread that reads or writes the stream. // FIXME
                    threadEnabled.store(true);
                    dataThread = new std::thread(threadCallback, this);
                }
            }
        }
        LOGD("OboeAudioStream_start: start returning %d", result);
        return result;
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
            sineOscillators[i].amplitude.setValue(amplitude);
            sawtoothOscillators[i].amplitude.setValue(amplitude);
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

    void setActivityType(int activityType) {
        LOGD("%s(%d)", __func__, activityType);
        mActivityType = (ActivityType) activityType;
    }

    InputStreamCallbackAnalyzer  mInputAnalyzer;
    bool                         useCallback = true;
    bool                         callbackReturnStop = false;
    int                          callbackSize = 0;

private:

    // WARNING - must match order in strings.xml and OboeAudioOutputStream.java
    enum ToneType {
        SawPing = 0,
        Sine = 1,
        Impulse = 2,
        Sawtooth = 3
    };

    // WARNING - must match definitions in TestAudioActivity.java
    enum ActivityType {
        Undefined = -1,
        TestOutput = 0,
        TestInput = 1,
        TapToTone = 2,
        RecordPlay = 3,
        Echo = 4
    };

    oboe::AudioStream * getOutputStream();

    int32_t allocateStreamIndex();

    void configureForActivityType();

    void freeStreamIndex(int32_t streamIndex);

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

    static constexpr int         kMaxStreams = 8;
    oboe::AudioStream           *mOboeStreams[kMaxStreams]{};
    int32_t                      mFramesPerBurst = 0; // TODO per stream
    int32_t                      mChannelCount = 0; // TODO per stream
    int32_t                      mSampleRate = 0; // TODO per stream
    ToneType                     mToneType = ToneType::Sine;
    ActivityType                 mActivityType = ActivityType::Undefined;

    std::atomic<bool>            threadEnabled{false};
    std::thread                 *dataThread = nullptr;

    OboeStreamCallbackProxy      oboeCallbackProxy;
    std::vector<SineOscillator>  sineOscillators;
    std::vector<SawtoothOscillator>  sawtoothOscillators;

    ImpulseOscillator            impulseGenerator;
    SawPingGenerator             sawPingGenerator;
    oboe::AudioStream           *playbackStream = nullptr;

    std::unique_ptr<float []>               dataBuffer{};
    std::unique_ptr<ManyToMultiConverter>   manyToMulti;
    std::unique_ptr<MonoToMultiConverter>   monoToMulti;
    std::shared_ptr<flowgraph::SinkFloat>   mSinkFloat;
    std::shared_ptr<flowgraph::SinkI16>     mSinkI16;
    std::unique_ptr<AudioStreamGateway>     audioStreamGateway{};
    std::unique_ptr<MultiChannelRecording>  mRecording{};

    PlayRecordingCallback        mPlayRecordingCallback;

    bool                       (*mAAudioStream_isMMap)(AAudioStream *stream) = nullptr;
    void                        *mLibHandle = nullptr;

};

#endif //NATIVEOBOE_NATIVEAUDIOCONTEXT_H
