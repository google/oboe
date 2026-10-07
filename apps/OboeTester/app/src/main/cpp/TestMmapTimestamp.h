/*
 * Copyright 2026 The Android Open Source Project
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

#ifndef OBOETESTER_TEST_MMAP_TIMESTAMP_H
#define OBOETESTER_TEST_MMAP_TIMESTAMP_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "oboe/Oboe.h"

/**
 * Diagnostic suite for MMAP audio timestamp accuracy, DMA ring-buffer position reporting,
 * rapid start/stop/pause/flush continuity, and idle standby (>3s) resume behavior.
 */
class TestMmapTimestamp {
public:
    static constexpr int32_t RESULT_RUNNING = 0;
    static constexpr int32_t RESULT_PASS = 1;
    static constexpr int32_t RESULT_WARN = 2;
    static constexpr int32_t RESULT_FAIL = -1;

    struct ConfigFlags {
        bool testOutput = true;
        bool testInput = true;
        bool testExclusive = true;
        bool testShared = true;
        bool runSteadyState = true;
        bool runRapidCycles = true;
        bool runStandbyResume = true;
    };

    ~TestMmapTimestamp() {
        stop();
    }

    int32_t start(const ConfigFlags &flags);
    int32_t stop();

    bool isRunning() const {
        return mIsRunning.load();
    }

    int32_t getResult() const {
        return mResult.load();
    }

    std::string getReport() const;

private:
    struct TimestampSample {
        int64_t hostTimeNs = 0;
        int64_t tsPosition = 0;
        int64_t tsTimeNs = 0;
        int64_t framesWritten = 0;
        int64_t framesRead = 0;
        int64_t callbackFrames = 0;
    };

    struct StreamScoreRow {
        std::string streamLabel;
        std::string configSummary;
        std::string steadyStatus = "SKIP";
        std::string rapidCyclesStatus = "SKIP";
        std::string standbyStatus = "SKIP";
    };

    struct Finding {
        std::string id;
        std::string severity;      // "FAIL" or "WARNING"
        std::string title;
        std::vector<std::string> affectedStreams;
        std::string whatHappened;
        std::vector<std::string> measuredEvidence;
        std::string androidContract;
        std::string whyItMatters;
        std::string howToFix;
    };

    class MyDataCallback : public oboe::AudioStreamDataCallback {
    public:
        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

        void reset() {
            mCallbackCount.store(0);
            mTotalCallbackFrames.store(0);
            mFirstCallbackTimeNs.store(0);
            mPhase = 0.0f;
        }

        int32_t getCallbackCount() const {
            return mCallbackCount.load();
        }

        int64_t getTotalCallbackFrames() const {
            return mTotalCallbackFrames.load();
        }

        int64_t getFirstCallbackTimeNs() const {
            return mFirstCallbackTimeNs.load();
        }

    private:
        std::atomic<int32_t> mCallbackCount{0};
        std::atomic<int64_t> mTotalCallbackFrames{0};
        std::atomic<int64_t> mFirstCallbackTimeNs{0};
        float mPhase = 0.0f;
        static constexpr float kPhaseIncrement = 2.0f * static_cast<float>(M_PI) * 440.0f / 48000.0f;
    };

    void runSuite(ConfigFlags flags);
    void runSingleConfig(oboe::Direction direction,
                         oboe::SharingMode sharingMode,
                         const ConfigFlags &flags,
                         size_t rowIndex);

    oboe::Result openMmapStream(oboe::Direction direction,
                                oboe::SharingMode sharingMode);
    void closeStream();

    void runSteadyStateCheck(oboe::Direction direction, size_t rowIndex);
    void runRapidCyclesCheck(oboe::Direction direction,
                             bool usePauseFlush,
                             size_t rowIndex,
                             bool *inoutAnyFailed,
                             std::string *inoutFailureTag);
    void runStandbyResumeCheck(oboe::Direction direction,
                               bool usePauseFlush,
                               size_t rowIndex,
                               bool *inoutAnyFailed,
                               std::string *inoutFailureTag);

    bool sleepMillisInterruptible(int32_t durationMs);
    bool runUntilUnalignedPosition(int32_t minRunMs, int32_t bufferCapacity, int32_t framesPerBurst);
    bool waitForFreshTimestamp(int64_t prevTsTimeNs,
                               int32_t timeoutMs,
                               TimestampSample *outSample,
                               oboe::Result *outLastResult = nullptr);

    void appendTelemetryLine(const std::string &line);
    void setScorecardCell(size_t rowIndex,
                          int phaseIndex,
                          const std::string &status);
    void setScorecardConfig(size_t rowIndex, const std::string &configSummary);
    void addOrUpdateFinding(const Finding &finding,
                            const std::string &streamDesc,
                            const std::string &evidenceLine);
    void recordPass();
    void recordWarn();
    void recordFail();
    std::string buildFullReportLocked(bool suiteCompleted) const;

    mutable std::mutex mReportMutex;
    std::string mTelemetryLog;
    std::string mCurrentProgress;
    std::vector<StreamScoreRow> mScorecard;
    std::vector<Finding> mFindings;
    bool mSuiteCompleted = false;
    bool mSuiteStoppedByUser = false;

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;

    std::atomic<bool> mThreadEnabled{false};
    std::atomic<bool> mIsRunning{false};
    std::atomic<int32_t> mResult{RESULT_RUNNING};
    std::thread mWorkerThread;

    int32_t mPassCount = 0;
    int32_t mWarnCount = 0;
    int32_t mFailCount = 0;

    static constexpr int32_t kChannelCount = 2;
    static constexpr int32_t kRapidCycleCount = 5;
    static constexpr int32_t kStandbySleepMs = 3500;
};

#endif // OBOETESTER_TEST_MMAP_TIMESTAMP_H
