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

#ifndef AUDIO_WORKLOAD_TEST_RUNNER_H
#define AUDIO_WORKLOAD_TEST_RUNNER_H

#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include "AudioWorkloadTest.h"

class AudioWorkloadTestRunner {
public:
    AudioWorkloadTestRunner();
    ~AudioWorkloadTestRunner();

    int32_t start(
            int32_t numCallbacks,
            int32_t bufferSizeInBursts,
            int32_t numVoices,
            int32_t highNumVoices,
            int32_t highLowPeriodMillis,
            bool adpfEnabled,
            bool sineEnabled);
    bool pollIsDone();
    std::string getStatus() const;
    int32_t stop();
    int32_t getResult() const;
    std::string getResultText() const;

private:
    std::unique_ptr<AudioWorkloadTest> mAudioWorkloadTest;
    std::atomic<bool> mIsRunning{false};
    std::atomic<bool> mIsDone{false};
    std::atomic<int32_t> mResult{0}; // 0: not finished, 1: pass, -1: fail
    std::string mResultText;
    int32_t mNumCallbacks;
    std::chrono::time_point<std::chrono::steady_clock> mStartTime;
};

#endif // AUDIO_WORKLOAD_TEST_RUNNER_H
