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

/**
 * @class AudioWorkloadTest
 * @brief A class designed to test audio workload performance using the Oboe library.
 *
 * This class sets up an audio stream, generates a synthetic audio load (sine wave and/or
 * a more complex synth workload), and collects statistics about the audio callback performance,
 * such as callback duration, XRun counts, and CPU usage.
 *
 * Example usage:
 * open();
 * start();
 * while (isRunning()) { getXRunCount(); getCallbackCount(); getLastDurationNs(); sleep(); }
 * getCallbackStatistics();
 * close();
 *
 */
class AudioWorkloadTest : oboe::AudioStreamDataCallback {
public:
    /**
     * @struct CallbackStatus
     * @brief Structure to store statistics for each audio callback invocation.
     */
    struct CallbackStatus {
        int32_t numVoices;      // Number of synthesizer voices active during this callback
        int64_t beginTimeNs;    // Timestamp (nanoseconds) when the callback started
        int64_t finishTimeNs;   // Timestamp (nanoseconds) when the callback finished
        int32_t xRunCount;      // Cumulative XRun (underrun/overrun) count at this point
        int32_t cpuIndex;       // CPU core index on which the callback executed
    };

    /**
     * @brief Constructor for AudioWorkloadTest.
     * Initializes the audio stream pointer to nullptr.
     */
    AudioWorkloadTest();

    /**
     * @brief Opens an audio stream with specified parameters.
     * Configures the stream for low latency output.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t open();

    /**
     * @brief Gets the number of frames processed in a single audio callback burst.
     * @return The number of frames per burst.
     */
    int32_t getFramesPerBurst() const;

    /**
     * @brief Gets the sample rate of the audio stream.
     * @return The sample rate in Hz.
     */
    int32_t getSampleRate() const;

    /**
     * @brief Gets the current buffer size of the audio stream in frames.
     * @return The buffer size in frames.
     */
    int32_t getBufferSizeInFrames() const;

    /**
     * @brief Starts the audio stream and the workload test.
     * @param targetDurationMillis The desired duration of the test in milliseconds.
     * @param bufferSizeInBursts The desired buffer size in terms of multiples of framesPerBurst.
     * @param numVoices The primary number of synthesizer voices to simulate.
     * @param alternateNumVoices An alternative number of voices for alternating workload.
     * @param alternatingPeriodMs The period in milliseconds to alternate between numVoices and
     * alternateNumVoices.
     * @param adpfEnabled Whether to enable Adaptive Performance (ADPF) hints.
     * @param hearWorkload If true, the synthesized audio will be audible; otherwise, it's processed
     * silently.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t start(int32_t targetDurationMillis, int32_t bufferSizeInBursts, int32_t numVoices,
                  int32_t alternateNumVoices, int32_t alternatingPeriodMs, bool adpfEnabled,
                  bool hearWorkload);

    /**
     * @brief Gets the number of available CPU cores on the system.
     * @return The number of CPU cores.
     */
    static int32_t getCpuCount();

    /**
     * @brief Sets the CPU affinity for the current thread (intended for the audio callback
     * thread).
     * @param mask A bitmask specifying the allowed CPU cores.
     * @return 0 on success, -1 on failure.
     */
    static int32_t setCpuAffinityForCallback(uint32_t mask);

    /**
     * @brief Gets the number of XRuns (underruns/overruns) that occurred during the last test run.
     * @return The XRun count.
     */
    int32_t getXRunCount() const;

    /**
     * @brief Gets the total number of audio callbacks invoked during the last test run.
     * @return The callback count.
     */
    int32_t getCallbackCount() const;

    /**
     * @brief Gets the duration of the last audio callback in nanoseconds.
     * @return The duration in nanoseconds.
     */
    int64_t getLastDurationNs();

    /**
     * @brief Checks if the audio workload test is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning();

    /**
     * @brief Stops the audio stream.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t stop();

    /**
     * @brief Closes the audio stream and releases resources.
     * @return 0 on success.
     */
    int32_t close();

    /**
     * @brief Retrieves the collected statistics for each audio callback.
     * Call this only after the stream is stopped as this is not atomic.
     * @return A vector of CallbackStatus structures.
     */
    std::vector<CallbackStatus> getCallbackStatistics();

    /**
     * @brief The Oboe audio callback function.
     * This function is called by the Oboe library when it needs more audio data.
     * It generates audio, performs workload simulation, and collects statistics.
     * @param audioStream Pointer to the Oboe audio stream.
     * @param audioData Pointer to the buffer where audio data should be written.
     * @param numFrames The number of audio frames to be filled.
     * @return oboe::DataCallbackResult::Continue to continue streaming, or
     * oboe::DataCallbackResult::Stop to stop.
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* audioStream, void* audioData,
                                          int32_t numFrames) override;

private:
    // Member variables
    oboe::AudioStream* mStream;              // Pointer to the Oboe audio stream instance

    // Atomic variables for thread-safe access from audio callback and other threads
    std::atomic<int32_t> mFramesPerBurst{0};
    std::atomic<int32_t> mSampleRate{0};
    std::atomic<int32_t> mCallbackCount{0};
    std::atomic<int32_t> mPreviousXRunCount{0};
    std::atomic<int32_t> mXRunCount{0};
    std::atomic<int32_t> mTargetDurationMs{0};
    std::atomic<int32_t> mBufferSizeInBursts{0};
    std::atomic<int32_t> mBufferSizeInFrames{0};
    std::atomic<int32_t> mNumVoices{0};
    std::atomic<int32_t> mAlternateNumVoices{0};
    std::atomic<int32_t> mAlternatingPeriodMs{0};
    std::atomic<int64_t> mLastDurationNs{0};
    std::atomic<int64_t> mStartTimeMs{0};
    std::atomic<bool> mHearWorkload{false};

    std::vector<CallbackStatus> mCallbackStatistics;
    std::atomic<bool> mRunning{false};

    // Sine wave generation parameters
    std::atomic<float> mPhase{0.0f};           // Current phase of the sine wave oscillator
    // Phase increment for a 440 Hz sine wave at a 48000 Hz sample rate
    static constexpr float kPhaseIncrement = 2.0f * (float) M_PI * 440.0f / 48000.0f;

    SynthWorkload mSynthWorkload;              // Instance of the synthetic workload generator
};

#endif // AUDIO_WORKLOAD_TEST_H
