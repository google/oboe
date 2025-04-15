/*
 * Copyright 2025 The Android Open Source Project
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

#ifndef AUDIO_WORKLOAD_TEST_H
#define AUDIO_WORKLOAD_TEST_H

#include <oboe/Oboe.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <unistd.h> // For CPU affinity
#include "SynthWorkload.h"

class AudioWorkloadTest : oboe::AudioStreamDataCallback {
public:
    struct CallbackStatus {
        int32_t numVoices;
        int64_t beginTimeNs;
        int64_t finishTimeNs;
        int32_t xRunCount;
        int32_t cpuIndex;
    };

    AudioWorkloadTest();
    int32_t open();
    int32_t getFramesPerBurst() const;
    int32_t getSampleRate() const;
    int32_t getBufferSizeInFrames() const;
    int32_t start(int32_t numCallbacks, int32_t bufferSizeInBursts, int32_t numVoices, int32_t alternateNumVoices, int32_t alternatingPeriodMs, bool adpfEnabled, bool sineEnabled);
    static int32_t getCpuCount();
    static int32_t setCpuAffinityForCallback(uint32_t mask);
    int32_t getXRunCount();
    int32_t getCallbackCount();
    int64_t getLastDurationNs();
    bool isRunning();
    int32_t stop();
    int32_t close();
    std::vector<CallbackStatus> getCallbackStatistics();
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames) override;

private:
    oboe::AudioStream* mStream;
    std::atomic<int32_t> mFramesPerBurst{0};
    std::atomic<int32_t> mSampleRate{0};
    std::atomic<int32_t> mCallbackCount{0};
    std::atomic<int32_t> mPreviousXRunCount{0};
    std::atomic<int32_t> mXRunCount{0};
    std::atomic<int32_t> mNumCallbacks{0};
    std::atomic<int32_t> mBufferSizeInBursts{0};
    std::atomic<int32_t> mBufferSizeInFrames{0};
    std::atomic<int32_t> mNumVoices{0};
    std::atomic<int32_t> mAlternateNumVoices{0};
    std::atomic<int32_t> mAlternatingPeriodMs{0};
    std::atomic<int64_t> mLastDurationNs{0};
    std::atomic<int64_t> mStartTimeMs{0};
    std::atomic<bool> mSineEnabled{false};
    std::vector<CallbackStatus> mCallbackStatistics;
    std::atomic<bool> mRunning{false};
    SynthWorkload mSynthWorkload;
};

#endif // AUDIO_WORKLOAD_TEST_H
