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

/**
 * @class AudioWorkloadTestRunner
 * @brief Manages the execution and results of an AudioWorkloadTest.
 *
 * This class encapsulates an AudioWorkloadTest instance, controls its lifecycle (start, stop),
 * monitors its progress, and determines a pass/fail result based on criteria like XRuns.
 */
class AudioWorkloadTestRunner {
public:
    /**
     * @brief Default constructor.
     */
    AudioWorkloadTestRunner() = default;

    /**
     * @brief Destructor. Ensures the test is stopped when the runner is destroyed.
     */
    ~AudioWorkloadTestRunner();

    /**
     * @brief Starts the audio workload test with the specified parameters.
     * @param targetDurationMs The desired duration of the test in milliseconds.
     * @param bufferSizeInBursts The desired buffer size in terms of multiples of framesPerBurst.
     * @param numVoices The primary number of synthesizer voices to simulate.
     * @param highNumVoices An alternative number of voices for alternating workload.
     * @param highLowPeriodMillis The period in milliseconds to alternate between numVoices and highNumVoices.
     * @param adpfEnabled Whether to enable Adaptive Performance (ADPF) hints.
     * @param hearWorkload If true, the synthesized audio will be audible; otherwise, it's processed silently.
     * @return 0 on success, -1 on failure (e.g., test already running, error opening/starting stream).
     */
    int32_t start(
            int32_t targetDurationMs,
            int32_t bufferSizeInBursts,
            int32_t numVoices,
            int32_t highNumVoices,
            int32_t highLowPeriodMillis,
            bool adpfEnabled,
            bool hearWorkload);

    /**
     * @brief Stops the test if it has completed its run (i.e., AudioWorkloadTest is no longer running).
     * This is useful for polling and cleaning up once the underlying test finishes due to duration.
     * @return True if the test is done (either stopped here or previously), false otherwise.
     */
    bool stopIfDone();

    /**
     * @brief Gets the current status of the test.
     * This can indicate if it's running, how many callbacks have occurred, or the final result text.
     * @return A string describing the current status.
     */
    std::string getStatus() const;

    /**
     * @brief Stops the audio workload test explicitly.
     * This will also finalize the test result.
     * @return 0 if the test was running and is now stopped, -1 if it was not running.
     */
    int32_t stop();

    /**
     * @brief Gets the numerical result of the test.
     * @return 0 if the test is not finished, 1 if it passed, -1 if it failed.
     */
    int32_t getResult() const;

    /**
     * @brief Gets a descriptive string for the test result (e.g., "PASS", "FAIL: X xruns").
     * @return A string containing the result text.
     */
    std::string getResultText() const;

private:
    AudioWorkloadTest mAudioWorkloadTest;    // The actual audio workload test instance
    std::atomic<bool> mIsRunning{false};
    std::atomic<bool> mIsDone{false};
    std::atomic<int32_t> mResult{0};
    std::string mResultText;
};

#endif // AUDIO_WORKLOAD_TEST_RUNNER_H
