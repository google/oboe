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

#define MODULE_NAME "OboeTester"

#include "TestMmapTimestamp.h"

#include <aaudio/AAudioExtensions.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <unistd.h>

#include "common/OboeDebug.h"
#include "oboe/AudioClock.h"
#include "OboeTools.h"

using namespace oboe;

namespace {

std::string formatString(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

std::string formatString(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

double framesToMs(int64_t frames, int32_t sampleRate) {
    if (sampleRate <= 0) return 0.0;
    return (static_cast<double>(frames) * 1000.0) / static_cast<double>(sampleRate);
}

} // namespace

DataCallbackResult TestMmapTimestamp::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    const int32_t count = mCallbackCount.fetch_add(1);
    if (count == 0) {
        mFirstCallbackTimeNs.store(AudioClock::getNanoseconds());
    }
    mTotalCallbackFrames.fetch_add(numFrames);
    if (audioStream->getDirection() == Direction::Output) {
        const int32_t channelCount = audioStream->getChannelCount();
        if (audioStream->getFormat() == AudioFormat::Float) {
            float *floatData = static_cast<float *>(audioData);
            for (int32_t i = 0; i < numFrames; i++) {
                const float sample = std::sin(mPhase) * 0.05f;
                for (int32_t ch = 0; ch < channelCount; ch++) {
                    *floatData++ = sample;
                }
                mPhase += kPhaseIncrement;
                if (mPhase >= static_cast<float>(M_PI)) {
                    mPhase -= 2.0f * static_cast<float>(M_PI);
                }
            }
        } else if (audioStream->getFormat() == AudioFormat::I16) {
            int16_t *shortData = static_cast<int16_t *>(audioData);
            for (int32_t i = 0; i < numFrames; i++) {
                const int16_t sample = static_cast<int16_t>(std::sin(mPhase) * 1600.0f);
                for (int32_t ch = 0; ch < channelCount; ch++) {
                    *shortData++ = sample;
                }
                mPhase += kPhaseIncrement;
                if (mPhase >= static_cast<float>(M_PI)) {
                    mPhase -= 2.0f * static_cast<float>(M_PI);
                }
            }
        }
    }
    return DataCallbackResult::Continue;
}

int32_t TestMmapTimestamp::start(const ConfigFlags &flags) {
    stop();
    {
        std::lock_guard<std::mutex> lock(mReportMutex);
        mTelemetryLog.clear();
        mCurrentProgress = "Initializing MMAP diagnostic suite...";
        mScorecard.clear();
        mFindings.clear();
        mSuiteCompleted = false;
        mSuiteStoppedByUser = false;
        mPassCount = 0;
        mWarnCount = 0;
        mFailCount = 0;
    }
    mResult.store(RESULT_RUNNING);
    mThreadEnabled.store(true);
    mIsRunning.store(true);
    mWorkerThread = std::thread(&TestMmapTimestamp::runSuite, this, flags);
    return 0;
}

int32_t TestMmapTimestamp::stop() {
    mThreadEnabled.store(false);
    if (mWorkerThread.joinable()) {
        mWorkerThread.join();
    }
    mIsRunning.store(false);
    return 0;
}

std::string TestMmapTimestamp::getReport() const {
    std::lock_guard<std::mutex> lock(mReportMutex);
    return buildFullReportLocked(mSuiteCompleted);
}

void TestMmapTimestamp::appendTelemetryLine(const std::string &line) {
    LOGI("[MmapDiag] %s", line.c_str());
    std::lock_guard<std::mutex> lock(mReportMutex);
    mTelemetryLog += line;
    mTelemetryLog += "\n";
}

void TestMmapTimestamp::setScorecardCell(size_t rowIndex,
                                         int phaseIndex,
                                         const std::string &status) {
    std::lock_guard<std::mutex> lock(mReportMutex);
    if (rowIndex >= mScorecard.size()) return;
    if (phaseIndex == 1) {
        mScorecard[rowIndex].steadyStatus = status;
    } else if (phaseIndex == 2) {
        mScorecard[rowIndex].rapidCyclesStatus = status;
    } else if (phaseIndex == 3) {
        mScorecard[rowIndex].standbyStatus = status;
    }
    mCurrentProgress = formatString("%s - Phase %d: %s",
                                    mScorecard[rowIndex].streamLabel.c_str(),
                                    phaseIndex,
                                    status.c_str());
}

void TestMmapTimestamp::setScorecardConfig(size_t rowIndex, const std::string &configSummary) {
    std::lock_guard<std::mutex> lock(mReportMutex);
    if (rowIndex < mScorecard.size()) {
        mScorecard[rowIndex].configSummary = configSummary;
    }
}

void TestMmapTimestamp::addOrUpdateFinding(const Finding &finding,
                                           const std::string &streamDesc,
                                           const std::string &evidenceLine) {
    LOGW("[MmapDiag] ISSUE [%s] (%s): %s | %s",
         finding.id.c_str(),
         finding.severity.c_str(),
         finding.title.c_str(),
         evidenceLine.c_str());
    std::lock_guard<std::mutex> lock(mReportMutex);
    for (auto &existing : mFindings) {
        if (existing.id == finding.id) {
            if (finding.severity == "FAIL") {
                existing.severity = "FAIL";
            }
            if (std::find(existing.affectedStreams.begin(),
                          existing.affectedStreams.end(),
                          streamDesc) == existing.affectedStreams.end()) {
                existing.affectedStreams.push_back(streamDesc);
            }
            existing.measuredEvidence.push_back(evidenceLine);
            return;
        }
    }
    Finding f = finding;
    f.affectedStreams.push_back(streamDesc);
    f.measuredEvidence.push_back(evidenceLine);
    mFindings.push_back(std::move(f));
}

void TestMmapTimestamp::recordPass() {
    std::lock_guard<std::mutex> lock(mReportMutex);
    mPassCount++;
}

void TestMmapTimestamp::recordWarn() {
    std::lock_guard<std::mutex> lock(mReportMutex);
    mWarnCount++;
}

void TestMmapTimestamp::recordFail() {
    std::lock_guard<std::mutex> lock(mReportMutex);
    mFailCount++;
}

std::string TestMmapTimestamp::buildFullReportLocked(bool suiteCompleted) const {
    std::ostringstream out;

    const int32_t totalChecks = mPassCount + mWarnCount + mFailCount;
    std::string overallStr = "RUNNING...";
    if (suiteCompleted) {
        if (mSuiteStoppedByUser) {
            overallStr = "STOPPED (cancelled by user)";
        } else if (mFailCount > 0) {
            overallStr = formatString("FAIL (%zu issue%s detected)",
                                      mFindings.size(),
                                      mFindings.size() == 1 ? "" : "s");
        } else if (mWarnCount > 0) {
            overallStr = formatString("WARN (%zu issue%s detected)",
                                      mFindings.size(),
                                      mFindings.size() == 1 ? "" : "s");
        } else {
            overallStr = "PASS";
        }
    }

    out << "================================================\n";
    out << "1. SUMMARY & SCORECARD\n";
    out << "================================================\n";
    out << formatString("Overall Verdict : %s\n", overallStr.c_str());
    if (!suiteCompleted) {
        out << formatString("Current Status  : %s\n", mCurrentProgress.c_str());
    }
    out << formatString("Checks Executed : %d (%d PASS, %d WARN, %d FAIL)\n",
                        totalChecks, mPassCount, mWarnCount, mFailCount);
    out << formatString("MMAP Support    : MMap=%s | Excl=%s | Workarounds=OFF\n\n",
                        AAudioExtensions::getInstance().isMMapSupported() ? "YES" : "NO",
                        AAudioExtensions::getInstance().isMMapExclusiveSupported() ? "YES" : "NO");

    out << "SCORECARD BY STREAM:\n";
    for (const auto &row : mScorecard) {
        out << formatString("* %s (%s)\n",
                            row.streamLabel.c_str(),
                            row.configSummary.c_str());
        out << formatString("    [1] Steady-State & DMA : %s\n", row.steadyStatus.c_str());
        out << formatString("    [2] Rapid Stop/Start   : %s\n", row.rapidCyclesStatus.c_str());
        out << formatString("    [3] 3.5s Idle Standby  : %s\n", row.standbyStatus.c_str());
    }
    out << "\n";

    out << "================================================\n";
    out << "2. DETECTED ISSUES & HOW TO FIX\n";
    out << "================================================\n";
    if (mFindings.empty()) {
        if (suiteCompleted) {
            out << "No MMAP timestamp or position continuity issues detected.\n\n";
        } else {
            out << "No issues detected so far (running)...\n\n";
        }
    } else {
        for (size_t i = 0; i < mFindings.size(); ++i) {
            const Finding &f = mFindings[i];
            if (i > 0) {
                out << "------------------------------------------------\n";
            }
            out << "[ISSUE #" << (i + 1) << "] " << f.title << "\n";
            out << "- Severity : " << f.severity << "\n";

            std::string streamsJoined;
            for (size_t s = 0; s < f.affectedStreams.size(); ++s) {
                if (s > 0) streamsJoined += "; ";
                streamsJoined += f.affectedStreams[s];
            }
            out << "- Streams  : " << streamsJoined << "\n\n";
            out << "- What Happened:\n  " << f.whatHappened << "\n\n";
            out << "- Measured:\n";
            for (const auto &ev : f.measuredEvidence) {
                out << "  * " << ev << "\n";
            }
            out << "\n";
            out << "- Android Contract:\n  " << f.androidContract << "\n\n";
            out << "- Why It Matters:\n  " << f.whyItMatters << "\n\n";
            out << "- How to Fix:\n  " << f.howToFix << "\n\n";
        }
    }

    out << "================================================\n";
    out << "3. GLOSSARY & DETAILED STREAM TELEMETRY\n";
    out << "================================================\n";
    out << "Glossary:\n";
    out << "- app  : App callback frames (written Out / read In)\n";
    out << "- dma  : MMAP ring-buffer counter (Reply.hardware)\n";
    out << "- ts   : Speaker/Mic timestamp (Reply.observable)\n";
    out << "- @off : Stop offset in ring buffer (dma % capacity)\n";
    out << "- tsErr: Jump in ts vs app across transition (ideal=0)\n";
    out << "- dmaEr: Jump in dma vs app across transition (ideal=0)\n\n";
    out << mTelemetryLog;

    return out.str();
}

bool TestMmapTimestamp::sleepMillisInterruptible(int32_t durationMs) {
    constexpr int32_t kStepMs = 20;
    int32_t remainingMs = durationMs;
    while (remainingMs > 0 && mThreadEnabled.load()) {
        const int32_t sleepMs = std::min(kStepMs, remainingMs);
        AudioClock::sleepForNanos(static_cast<int64_t>(sleepMs) * NANOS_PER_MILLISECOND);
        remainingMs -= sleepMs;
    }
    return mThreadEnabled.load();
}

void TestMmapTimestamp::runSuite(ConfigFlags flags) {
    const bool prevWorkarounds = OboeGlobals::areWorkaroundsEnabled();
    OboeGlobals::setWorkaroundsEnabled(false);

    struct StreamPlan {
        Direction dir;
        SharingMode sharing;
        std::string label;
    };
    std::vector<StreamPlan> plans;
    if (flags.testOutput && flags.testExclusive) {
        plans.push_back({Direction::Output, SharingMode::Exclusive, "OUTPUT EXCLUSIVE"});
    }
    if (flags.testOutput && flags.testShared) {
        plans.push_back({Direction::Output, SharingMode::Shared, "OUTPUT SHARED"});
    }
    if (flags.testInput && flags.testExclusive) {
        plans.push_back({Direction::Input, SharingMode::Exclusive, "INPUT EXCLUSIVE"});
    }
    if (flags.testInput && flags.testShared) {
        plans.push_back({Direction::Input, SharingMode::Shared, "INPUT SHARED"});
    }

    {
        std::lock_guard<std::mutex> lock(mReportMutex);
        for (const auto &p : plans) {
            StreamScoreRow row;
            row.streamLabel = p.label;
            row.configSummary = "Pending...";
            row.steadyStatus = flags.runSteadyState ? "PENDING" : "SKIP";
            row.rapidCyclesStatus = flags.runRapidCycles ? "PENDING" : "SKIP";
            row.standbyStatus = flags.runStandbyResume ? "PENDING" : "SKIP";
            mScorecard.push_back(std::move(row));
        }
    }

    for (size_t i = 0; i < plans.size() && mThreadEnabled.load(); ++i) {
        runSingleConfig(plans[i].dir, plans[i].sharing, flags, i);
    }

    {
        std::lock_guard<std::mutex> lock(mReportMutex);
        mSuiteCompleted = true;
        if (!mThreadEnabled.load()) {
            mSuiteStoppedByUser = true;
            mCurrentProgress = "Stopped by user.";
            for (auto &row : mScorecard) {
                if (row.steadyStatus == "PENDING" || row.steadyStatus == "RUNNING...") {
                    row.steadyStatus = "STOPPED";
                }
                if (row.rapidCyclesStatus == "PENDING" || row.rapidCyclesStatus == "RUNNING...") {
                    row.rapidCyclesStatus = "STOPPED";
                }
                if (row.standbyStatus == "PENDING" || row.standbyStatus == "RUNNING...") {
                    row.standbyStatus = "STOPPED";
                }
            }
        } else {
            mCurrentProgress = "Completed.";
        }
        if (mFailCount > 0) {
            mResult.store(RESULT_FAIL);
        } else if (mWarnCount > 0) {
            mResult.store(RESULT_WARN);
        } else {
            mResult.store(RESULT_PASS);
        }
        const std::string fullReport = buildFullReportLocked(true);
        std::istringstream iss(fullReport);
        std::string line;
        while (std::getline(iss, line)) {
            LOGI("[MmapReport] %s", line.c_str());
        }
    }

    OboeGlobals::setWorkaroundsEnabled(prevWorkarounds);
    mIsRunning.store(false);
}

Result TestMmapTimestamp::openMmapStream(Direction direction, SharingMode sharingMode) {
    closeStream();
    mDataCallback = std::make_shared<MyDataCallback>();

    AAudioExtensions::getInstance().setMMapEnabled(true);

    AudioStreamBuilder builder;
    builder.setAudioApi(AudioApi::AAudio)
            ->setDirection(direction)
            ->setSharingMode(sharingMode)
            ->setPerformanceMode(PerformanceMode::LowLatency)
            ->setFormat(AudioFormat::Float)
            ->setChannelCount(kChannelCount)
            ->setDataCallback(mDataCallback);
    if (direction == Direction::Input) {
        builder.setInputPreset(InputPreset::Unprocessed);
    }

    return builder.openStream(mStream);
}

void TestMmapTimestamp::closeStream() {
    if (mStream) {
        mStream->stop();
        mStream->close();
        mStream.reset();
    }
}

void TestMmapTimestamp::runSingleConfig(Direction direction,
                                        SharingMode sharingMode,
                                        const ConfigFlags &flags,
                                        size_t rowIndex) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const char *shareStr = (sharingMode == SharingMode::Exclusive) ? "EXCLUSIVE" : "SHARED";
    const std::string streamName = formatString("%s %s", dirStr, shareStr);

    appendTelemetryLine("------------------------------------------------");
    appendTelemetryLine(formatString("STREAM: %s", streamName.c_str()));
    appendTelemetryLine("------------------------------------------------");

    Result result = openMmapStream(direction, sharingMode);
    if (result != Result::OK || !mStream) {
        setScorecardConfig(rowIndex, formatString("openStream failed (%s)", convertToText(result)));
        setScorecardCell(rowIndex, 1, "SKIP");
        setScorecardCell(rowIndex, 2, "SKIP");
        setScorecardCell(rowIndex, 3, "SKIP");
        appendTelemetryLine(formatString("  openStream failed: %s (%d) [SKIP]\n",
                                         convertToText(result), static_cast<int>(result)));
        return;
    }

    const bool isMmap = AAudioExtensions::getInstance().isMMapUsed(mStream.get());
    const SharingMode actualSharing = mStream->getSharingMode();
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t burst = mStream->getFramesPerBurst();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t deviceId = mStream->getDeviceId();
    const double capMs = framesToMs(capacity, sampleRate);
    const double burstMs = framesToMs(burst, sampleRate);

    setScorecardConfig(rowIndex,
                       formatString("%dkHz, buf=%df/%.0fms, burst=%df",
                                    sampleRate / 1000, capacity, capMs, burst));

    appendTelemetryLine(formatString(
            "  Config: MMAP=%s | Mode=%s | DevId=%d",
            isMmap ? "YES" : "NO",
            (actualSharing == SharingMode::Exclusive) ? "EXCLUSIVE" : "SHARED",
            deviceId));
    appendTelemetryLine(formatString(
            "  Buffer: %d Hz | %df (%.1fms) | Burst=%df (%.1fms)\n",
            sampleRate, capacity, capMs, burst, burstMs));

    if (!isMmap) {
        setScorecardConfig(rowIndex, "Non-MMAP (Fallback)");
        setScorecardCell(rowIndex, 1, "SKIP");
        setScorecardCell(rowIndex, 2, "SKIP");
        setScorecardCell(rowIndex, 3, "SKIP");
        appendTelemetryLine("  [SKIP] Stream did not open as MMAP.\n");
        closeStream();
        return;
    }
    if (sharingMode == SharingMode::Exclusive && actualSharing != SharingMode::Exclusive) {
        appendTelemetryLine("  [NOTE] Requested EXCLUSIVE fell back to SHARED.\n");
    }

    const std::string streamDesc = formatString("%s %s (%d Hz, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate,
                                                capacity, capMs, burst);

    auto recordReopenFailure = [&](const char *phaseLabel,
                                   bool *inoutFailed,
                                   std::string *inoutTag,
                                   Result openRes) {
        *inoutFailed = true;
        *inoutTag = "FAIL (reopen failed)";
        Finding f;
        f.id = "STREAM-REOPEN-OR-START-FAILURE";
        f.severity = "FAIL";
        f.title = "MMAP stream failed to reopen or start during diagnostic phase";
        f.whatHappened = "Reopening or starting the MMAP stream for a subsequent test phase failed or fell back to non-MMAP.";
        f.androidContract = "AAudio.h: Opening and starting a valid MMAP stream configuration after closing a previous stream must succeed and remain on the MMAP path.";
        f.whyItMatters = "Indicates a resource leak or unreleased hardware subdevice when closing or restarting MMAP streams.";
        f.howToFix = "Ensure the HAL and audio service release all PCM handles and shared memory file descriptors when an MMAP stream is stopped and closed.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s (%s): openMmapStream returned %s.",
                                        dirStr, shareStr, phaseLabel, convertToText(openRes)));
        recordFail();
    };

    if (flags.runSteadyState && mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 1, "RUNNING...");
        runSteadyStateCheck(direction, rowIndex);
    }

    if (flags.runRapidCycles && mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 2, "RUNNING...");
        bool rapidFailed = false;
        std::string rapidFailureTag = "FAIL";
        if (direction == Direction::Output && mThreadEnabled.load()) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get())) {
                runRapidCyclesCheck(direction, true /* usePauseFlush */, rowIndex,
                                    &rapidFailed, &rapidFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure("Phase 2 Pause/Flush", &rapidFailed, &rapidFailureTag, openRes);
            }
        }
        if (mThreadEnabled.load()) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get())) {
                runRapidCyclesCheck(direction, false /* usePauseFlush (stop->start) */, rowIndex,
                                    &rapidFailed, &rapidFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure("Phase 2 Stop/Start", &rapidFailed, &rapidFailureTag, openRes);
            }
        }
        if (mThreadEnabled.load()) {
            setScorecardCell(rowIndex, 2, rapidFailed ? rapidFailureTag : "PASS");
        } else {
            setScorecardCell(rowIndex, 2, "STOPPED");
        }
    }

    if (flags.runStandbyResume && mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 3, "RUNNING...");
        bool standbyFailed = false;
        std::string standbyFailureTag = "FAIL";
        if (direction == Direction::Output && mThreadEnabled.load()) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get())) {
                runStandbyResumeCheck(direction, true /* usePauseFlush */, rowIndex,
                                      &standbyFailed, &standbyFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure("Phase 3 Pause/Standby", &standbyFailed, &standbyFailureTag, openRes);
            }
        }
        if (mThreadEnabled.load()) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get())) {
                runStandbyResumeCheck(direction, false /* usePauseFlush (stop->standby->start) */, rowIndex,
                                      &standbyFailed, &standbyFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure("Phase 3 Stop/Standby", &standbyFailed, &standbyFailureTag, openRes);
            }
        }
        if (mThreadEnabled.load()) {
            setScorecardCell(rowIndex, 3, standbyFailed ? standbyFailureTag : "PASS");
        } else {
            setScorecardCell(rowIndex, 3, "STOPPED");
        }
    }

    closeStream();
    appendTelemetryLine("");
}

void TestMmapTimestamp::runSteadyStateCheck(Direction direction, size_t rowIndex) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const char *shareStr = (mStream->getSharingMode() == SharingMode::Exclusive) ? "EXCLUSIVE" : "SHARED";
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t burst = mStream->getFramesPerBurst();
    const std::string streamDesc = formatString("%s %s (%d Hz, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate,
                                                capacity, framesToMs(capacity, sampleRate), burst);

    appendTelemetryLine("  [Phase 1] Steady-State & Cold-Start DMA (1.5s)");
    mDataCallback->reset();

    const int64_t startCallNs = AudioClock::getNanoseconds();
    Result res = mStream->start();
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: start() returned %s\n", convertToText(res)));
        setScorecardCell(rowIndex, 1, "FAIL (start failed)");
        Finding f;
        f.id = "STEADY-START-OR-TIMESTAMP-UNAVAILABLE";
        f.severity = "FAIL";
        f.title = "MMAP stream failed to start or produce valid timestamps during steady-state run";
        f.whatHappened = "Starting the MMAP stream or polling getTimestamp(CLOCK_MONOTONIC) during 1.5s of active streaming failed.";
        f.androidContract = "AAudio.h: An open MMAP stream must start cleanly and provide advancing (framePosition, timeNanoseconds) pairs via AAudioStream_getTimestamp().";
        f.whyItMatters = "Applications cannot play/capture audio or synchronize audio/video clocks without a working MMAP stream and valid timestamps.";
        f.howToFix = "Verify that the HAL starts the PCM DMA stream on IStreamCommon::sendCommand(start/burst) and populates Reply.observable with valid (frames, timeNs) pairs.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: start() returned %s.",
                                        dirStr, shareStr, convertToText(res)));
        recordFail();
        return;
    }

    int64_t firstHwAdvanceNs = 0;
    std::vector<TimestampSample> samples;
    samples.reserve(450);

    int32_t validTimestampPolls = 0;
    int32_t staleFrameNewTimeCount = 0;
    double maxStaleFrameDeltaMs = 0.0;
    int64_t staleFrameExamplePos = 0;
    int32_t backwardPosCount = 0;
    int32_t backwardTimeCount = 0;
    int32_t rateOutlierCount = 0;
    double minInstRate = 1e12;
    double maxInstRate = 0.0;

    TimestampSample prevValidSample{};
    bool hasPrevValid = false;

    const int64_t testEndNs = startCallNs + 1500LL * NANOS_PER_MILLISECOND;
    while (AudioClock::getNanoseconds() < testEndNs && mThreadEnabled.load()) {
        const int64_t nowNs = AudioClock::getNanoseconds();
        const int64_t fw = mStream->getFramesWritten();
        const int64_t fr = mStream->getFramesRead();
        const int64_t hwFrames = (direction == Direction::Output) ? fr : fw;

        if (firstHwAdvanceNs == 0 && hwFrames > 0) {
            firstHwAdvanceNs = nowNs;
        }

        int64_t tsPos = 0;
        int64_t tsTime = 0;
        if (mStream->getTimestamp(CLOCK_MONOTONIC, &tsPos, &tsTime) == Result::OK) {
            validTimestampPolls++;
            TimestampSample s{nowNs, tsPos, tsTime, fw, fr};

            if (hasPrevValid) {
                const int64_t dPos = s.tsPosition - prevValidSample.tsPosition;
                const int64_t dTimeNs = s.tsTimeNs - prevValidSample.tsTimeNs;

                if (dPos < 0) backwardPosCount++;
                if (dTimeNs < 0) backwardTimeCount++;

                if (dPos == 0 && dTimeNs > 0) {
                    staleFrameNewTimeCount++;
                    const double deltaMs = static_cast<double>(dTimeNs) / NANOS_PER_MILLISECOND;
                    if (deltaMs > maxStaleFrameDeltaMs) {
                        maxStaleFrameDeltaMs = deltaMs;
                        staleFrameExamplePos = s.tsPosition;
                    }
                } else if (dPos > 0 && dTimeNs > 0) {
                    const double instRate =
                            (static_cast<double>(dPos) * NANOS_PER_SECOND) /
                            static_cast<double>(dTimeNs);
                    minInstRate = std::min(minInstRate, instRate);
                    maxInstRate = std::max(maxInstRate, instRate);
                    if (instRate < 0.5 * sampleRate || instRate > 1.5 * sampleRate) {
                        rateOutlierCount++;
                    }
                    samples.push_back(s);
                }
            } else {
                samples.push_back(s);
                hasPrevValid = true;
            }
            prevValidSample = s;
        }

        const int64_t elapsedMs = (nowNs - startCallNs) / NANOS_PER_MILLISECOND;
        const int32_t sleepMs = (elapsedMs < 100) ? 2 : 4;
        AudioClock::sleepForNanos(static_cast<int64_t>(sleepMs) * NANOS_PER_MILLISECOND);
    }

    if (!mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 1, "STOPPED");
        return;
    }

    const int64_t firstCbNs = mDataCallback->getFirstCallbackTimeNs();
    const double firstCbMs = (firstCbNs > 0)
            ? static_cast<double>(firstCbNs - startCallNs) / NANOS_PER_MILLISECOND
            : -1.0;
    const double firstHwMs = (firstHwAdvanceNs > 0)
            ? static_cast<double>(firstHwAdvanceNs - startCallNs) / NANOS_PER_MILLISECOND
            : -1.0;
    const double hwLagAfterCbMs = (firstCbNs > 0 && firstHwAdvanceNs > firstCbNs)
            ? static_cast<double>(firstHwAdvanceNs - firstCbNs) / NANOS_PER_MILLISECOND
            : 0.0;

    appendTelemetryLine(formatString(
            "    Cold-Start : 1stCb=%.1fms, 1stDma=%.1fms (lag=%.1fms)",
            firstCbMs, firstHwMs, hwLagAfterCbMs));
    appendTelemetryLine(formatString(
            "    Timestamps : %d valid, %zu distinct, %d stale-frame",
            validTimestampPolls, samples.size(), staleFrameNewTimeCount));

    if (samples.size() < 5) {
        appendTelemetryLine("    Result     : FAIL (<5 distinct timestamps)\n");
        setScorecardCell(rowIndex, 1, "FAIL (no timestamps)");
        Finding f;
        f.id = "STEADY-START-OR-TIMESTAMP-UNAVAILABLE";
        f.severity = "FAIL";
        f.title = "MMAP stream failed to start or produce valid timestamps during steady-state run";
        f.whatHappened = "During 1.5s of active streaming, getTimestamp(CLOCK_MONOTONIC) returned fewer than 5 distinct timestamp updates.";
        f.androidContract = "AAudio.h & StreamDescriptor.aidl (Reply.observable): An active MMAP stream must report advancing (frames, timeNs) presentation timestamps.";
        f.whyItMatters = "Without advancing timestamps, apps cannot compute audio latency, A/V lip-sync, or clock drift.";
        f.howToFix = "Ensure the HAL updates Reply.observable on status queries and does not leave the timestamp frozen or invalid while streaming.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: Only %zu distinct timestamps returned across %d valid polls in 1.5s.",
                                        dirStr, shareStr, samples.size(), validTimestampPolls));
        recordFail();
        return;
    }

    const double t0 = static_cast<double>(samples.front().tsTimeNs);
    const double p0 = static_cast<double>(samples.front().tsPosition);
    double sumT = 0.0;
    double sumP = 0.0;
    double sumTT = 0.0;
    double sumTP = 0.0;
    double sumPipelineOffsetFrames = 0.0;
    const double n = static_cast<double>(samples.size());

    for (const auto &s : samples) {
        const double tSec = (static_cast<double>(s.tsTimeNs) - t0) / NANOS_PER_SECOND;
        const double pFrames = static_cast<double>(s.tsPosition) - p0;
        sumT += tSec;
        sumP += pFrames;
        sumTT += tSec * tSec;
        sumTP += tSec * pFrames;

        const double dtHostSec =
                static_cast<double>(s.hostTimeNs - s.tsTimeNs) / NANOS_PER_SECOND;
        const int64_t hwCounter = (direction == Direction::Output) ? s.framesRead : s.framesWritten;
        const double hwAtTs = static_cast<double>(hwCounter) - dtHostSec * sampleRate;
        sumPipelineOffsetFrames += (hwAtTs - static_cast<double>(s.tsPosition));
    }

    const double denom = n * sumTT - sumT * sumT;
    const double slopeHz = (std::abs(denom) > 1e-12) ? ((n * sumTP - sumT * sumP) / denom) : 0.0;
    const double interceptFrames = (sumP - slopeHz * sumT) / n;
    const double rateErrorPpm = ((slopeHz - sampleRate) / sampleRate) * 1e6;

    double sumSqResidualFrames = 0.0;
    double maxAbsResidualFrames = 0.0;
    for (const auto &s : samples) {
        const double tSec = (static_cast<double>(s.tsTimeNs) - t0) / NANOS_PER_SECOND;
        const double pFrames = static_cast<double>(s.tsPosition) - p0;
        const double predicted = interceptFrames + slopeHz * tSec;
        const double residual = pFrames - predicted;
        sumSqResidualFrames += residual * residual;
        maxAbsResidualFrames = std::max(maxAbsResidualFrames, std::abs(residual));
    }

    const double rmsResidualFrames = std::sqrt(sumSqResidualFrames / n);
    const double rmsResidualMs = (rmsResidualFrames * 1000.0) / sampleRate;
    const double maxAbsResidualMs = (maxAbsResidualFrames * 1000.0) / sampleRate;
    const double avgPipelineOffsetFrames = sumPipelineOffsetFrames / n;
    const double avgPipelineOffsetMs = (avgPipelineOffsetFrames * 1000.0) / sampleRate;

    appendTelemetryLine(formatString(
            "    Clock Fit  : %.1f Hz (%+.0f ppm)",
            slopeHz, rateErrorPpm));
    appendTelemetryLine(formatString(
            "    Jitter     : RMS=%.2fms (%.1ff), Max=%.2fms (%.1ff)",
            rmsResidualMs, rmsResidualFrames, maxAbsResidualMs, maxAbsResidualFrames));
    appendTelemetryLine(formatString(
            "    %-10s : dma - ts = %+.1ff (%+.2f ms)",
            (direction == Direction::Output) ? "DMA-to-Spk" : "DMA-to-Mic",
            avgPipelineOffsetFrames, avgPipelineOffsetMs));

    bool checkFailed = false;
    bool checkWarned = false;
    std::string statusTag = "PASS";

    if (backwardPosCount > 0 || backwardTimeCount > 0) {
        checkFailed = true;
        statusTag = "FAIL (non-monotonic ts)";
        Finding f;
        f.id = "STEADY-NON-MONOTONIC-TIMESTAMP";
        f.severity = "FAIL";
        f.title = "Timestamp frame position or clock moved backward during active streaming";
        f.whatHappened = "During continuous 1.5s streaming without any stop or pause calls, consecutive getTimestamp() queries returned a smaller frame position or an earlier CLOCK_MONOTONIC timestamp than the preceding query.";
        f.androidContract = "AAudioStream_getTimestamp() and StreamDescriptor.aidl (Reply.observable): Both frame position and timeNs must be strictly non-decreasing while a stream is actively running.";
        f.whyItMatters = "Causes audio/video sync jumps, glitches, and erratic clock drift recovery in media playback and VoIP apps.";
        f.howToFix = "Ensure the HAL and audio service read and latch the hardware frame counter and CLOCK_MONOTONIC timestamp atomically without race conditions or integer wraparound.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: backwardPos=%d, backwardTime=%d during 1.5s steady run.",
                                        dirStr, shareStr, backwardPosCount, backwardTimeCount));
    }

    if (staleFrameNewTimeCount > 0) {
        checkFailed = true;
        statusTag = "FAIL (stale frame, new timeNs)";
        Finding f;
        f.id = "STEADY-STALE-FRAME-UPDATED-CLOCK";
        f.severity = "FAIL";
        f.title = "Timestamp clock (timeNs) advances when frame counter has not changed";
        f.whatHappened = "When getTimestamp() was polled between hardware burst updates, the reported frame count stayed identical to the previous poll, but timeNs was overwritten with the current system clock.";
        f.androidContract = "StreamDescriptor.aidl (Position): (frames, timeNs) must represent the exact moment the hardware last updated the frame counter. If frames has not changed since the previous query, timeNs must NOT be overwritten with the current polling time.";
        f.whyItMatters = "Produces sawtooth timestamp jitter equal to the hardware interrupt/burst period, degrading A/V lip-sync and WebRTC clock drift estimation.";
        f.howToFix = "Latch the (frames, timeNs) pair when the hardware DMA/DSP frame counter actually increments, or only update timeNs when the reported frame count changes.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: %d of %d polls returned an unchanged frame position (e.g. %lldf) while timeNs advanced by up to %.2f ms.",
                                        dirStr, shareStr, staleFrameNewTimeCount, validTimestampPolls,
                                        static_cast<long long>(staleFrameExamplePos), maxStaleFrameDeltaMs));
    }

    if (std::abs(rateErrorPpm) > 5000.0 || maxAbsResidualMs > 15.0) {
        const bool isFail = (std::abs(rateErrorPpm) > 20000.0);
        if (isFail) {
            checkFailed = true;
            statusTag = formatString("FAIL (rate %+.0fppm)", rateErrorPpm);
        } else if (!checkFailed) {
            checkWarned = true;
            statusTag = formatString("WARN (jitter %.1fms)", maxAbsResidualMs);
        }
        Finding f;
        f.id = "STEADY-CLOCK-DRIFT-OR-JITTER";
        f.severity = isFail ? "FAIL" : "WARNING";
        f.title = "Excessive steady-state timestamp rate error or linear-fit jitter";
        f.whatHappened = "During continuous 1.5s streaming, the linear regression slope of getTimestamp() deviated significantly from the nominal sample rate or exhibited high residual jitter.";
        f.androidContract = "AAudio.h (AAudioStream_getTimestamp): Reported (framePosition, timeNanoseconds) pairs should track the hardware sample clock smoothly with minimal jitter.";
        f.whyItMatters = "High timestamp jitter or clock rate error degrades A/V lip-sync, adaptive resampling, and acoustic echo cancellation.";
        f.howToFix = "Capture the hardware DMA counter and CLOCK_MONOTONIC timestamp together in the kernel/DSP interrupt or ALSA status ioctl rather than using delayed userspace polling timestamps.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: Linear fit rate=%.1f Hz (%+.0f ppm vs %d Hz), RMS jitter=%.2f ms, Max residual=%.2f ms (outliers=%d).",
                                        dirStr, shareStr, slopeHz, rateErrorPpm, sampleRate,
                                        rmsResidualMs, maxAbsResidualMs, rateOutlierCount));
    }

    if (direction == Direction::Output) {
        if (avgPipelineOffsetMs < -1.0 || (hwLagAfterCbMs >= 12.0 && std::abs(avgPipelineOffsetMs) < 2.0)) {
            const bool isFail = (avgPipelineOffsetMs < -1.0);
            if (isFail) {
                checkFailed = true;
                statusTag = "FAIL (dma < ts)";
            } else if (!checkFailed) {
                checkWarned = true;
                statusTag = formatString("WARN (dma == ts, %.0fms lag)", hwLagAfterCbMs);
            }
            Finding f;
            f.id = "STEADY-OUTPUT-DMA-SUBTRACTS-LATENCY";
            f.severity = isFail ? "FAIL" : "WARNING";
            f.title = "Output DMA read counter (getFramesRead) subtracts downstream speaker latency";
            f.whatHappened = "On an output MMAP stream, getFramesRead() tracks how many frames the hardware DMA engine has read from the shared ring buffer (Reply.hardware), whereas getTimestamp() reports how many frames have reached the external speaker (Reply.observable). On this device, getFramesRead() stayed clamped at 0 after the first callback while DMA was actively reading, and in steady state (getFramesRead() - getTimestamp().position) did not reflect positive downstream DSP/speaker latency.";
            f.androidContract = "StreamDescriptor.aidl (Reply): Reply.hardware must report the unadjusted DMA ring-buffer read position. Reply.observable must report the external speaker presentation position (Reply.hardware minus downstream DSP/speaker latency, or equivalently timestamped when those frames reach the speaker).";
            f.whyItMatters = "Audio clients use getFramesRead() (Reply.hardware) to compute how many unconsumed frames remain in the shared MMAP ring buffer (framesWritten - framesRead). Subtracting downstream speaker latency from Reply.hardware makes the ring buffer appear fuller than it actually is, clamping getFramesRead() at 0 during initial startup and skewing buffer occupancy and latency tuning.";
            f.howToFix = "1. When populating Reply.hardware (hardware DMA position), return the unshifted monotonic DMA transfer counter without subtracting downstream pipeline latency.\n  2. Account for downstream DSP/speaker latency only in Reply.observable (either by adding pipeline latency to Reply.observable.timeNs or subtracting latency frames from Reply.observable.frames, clamped so Reply.observable.frames never moves backward).";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s: getFramesRead() stayed at 0 for %.1f ms after 1st callback, and steady-state (getFramesRead() - getTimestamp().position) is %+.2f ms (%+.1f frames) instead of positive speaker latency.",
                                            dirStr, shareStr, hwLagAfterCbMs, avgPipelineOffsetMs, avgPipelineOffsetFrames));
        }
    }

    if (checkFailed) {
        appendTelemetryLine("    Result     : FAIL\n");
        setScorecardCell(rowIndex, 1, statusTag);
        recordFail();
    } else if (checkWarned) {
        appendTelemetryLine("    Result     : WARN\n");
        setScorecardCell(rowIndex, 1, statusTag);
        recordWarn();
    } else {
        appendTelemetryLine("    Result     : PASS\n");
        setScorecardCell(rowIndex, 1, "PASS");
        recordPass();
    }
}

bool TestMmapTimestamp::runUntilUnalignedPosition(int32_t minRunMs,
                                                  int32_t bufferCapacity,
                                                  int32_t framesPerBurst) {
    if (!sleepMillisInterruptible(minRunMs)) {
        return false;
    }
    if (bufferCapacity <= 0) {
        return true;
    }
    const int32_t minRem = std::min(bufferCapacity / 4, std::max(framesPerBurst, 48));
    for (int32_t attempt = 0; attempt < 40 && mThreadEnabled.load(); ++attempt) {
        const int64_t hw = (mStream->getDirection() == Direction::Output)
                ? mStream->getFramesRead()
                : mStream->getFramesWritten();
        const int64_t rem = (bufferCapacity > 0) ? (hw % bufferCapacity) : 0;
        if (rem >= minRem && rem <= (bufferCapacity - minRem)) {
            return true;
        }
        AudioClock::sleepForNanos(3 * NANOS_PER_MILLISECOND);
    }
    return mThreadEnabled.load();
}

bool TestMmapTimestamp::waitForFreshTimestamp(int64_t prevTsTimeNs,
                                              int32_t timeoutMs,
                                              TimestampSample *outSample,
                                              Result *outLastResult) {
    Result lastRes = Result::ErrorTimeout;
    const int64_t deadlineNs =
            AudioClock::getNanoseconds() + static_cast<int64_t>(timeoutMs) * NANOS_PER_MILLISECOND;
    while (AudioClock::getNanoseconds() < deadlineNs && mThreadEnabled.load()) {
        int64_t tsPos = 0;
        int64_t tsTime = 0;
        lastRes = mStream->getTimestamp(CLOCK_MONOTONIC, &tsPos, &tsTime);
        if (lastRes == Result::OK && tsTime > prevTsTimeNs) {
            outSample->hostTimeNs = AudioClock::getNanoseconds();
            outSample->tsPosition = tsPos;
            outSample->tsTimeNs = tsTime;
            outSample->framesWritten = mStream->getFramesWritten();
            outSample->framesRead = mStream->getFramesRead();
            outSample->callbackFrames = mDataCallback ? mDataCallback->getTotalCallbackFrames() : 0;
            if (outLastResult) *outLastResult = Result::OK;
            return true;
        }
        AudioClock::sleepForNanos(2 * NANOS_PER_MILLISECOND);
    }
    if (outLastResult) *outLastResult = lastRes;
    return false;
}

void TestMmapTimestamp::runRapidCyclesCheck(Direction direction,
                                            bool usePauseFlush,
                                            size_t /* rowIndex */,
                                            bool *inoutAnyFailed,
                                            std::string *inoutFailureTag) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const bool isShared = (mStream->getSharingMode() == SharingMode::Shared);
    const char *shareStr = isShared ? "SHARED" : "EXCLUSIVE";
    const char *modeStr = usePauseFlush ? "Pause -> Flush -> Start" : "Stop -> Start";
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t burst = mStream->getFramesPerBurst();
    const double capMs = framesToMs(capacity, sampleRate);
    const std::string streamDesc = formatString("%s %s (%d Hz, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate, capacity, capMs, burst);

    appendTelemetryLine(formatString("  [Phase 2] Rapid %s (%d cycles)",
                                     modeStr, kRapidCycleCount));

    auto recordCycleControlFinding = [&](const std::string &tag, const std::string &detail) {
        *inoutAnyFailed = true;
        *inoutFailureTag = tag;
        Finding f;
        f.id = "CYCLE-STREAM-CONTROL-OR-TIMESTAMP-FAILURE";
        f.severity = "FAIL";
        f.title = "Stream state transition or getTimestamp() failed during rapid Stop/Start cycles";
        f.whatHappened = "During rapid start/pause/flush/stop cycles, starting or stopping the stream failed or getTimestamp(CLOCK_MONOTONIC) failed to advance.";
        f.androidContract = "AAudio.h: Rapid start(), pause(), flush(), and stop() transitions must succeed and resume producing advancing timestamps.";
        f.whyItMatters = "Causes playback or capture to stall or lose timestamp synchronization after quick pause/resume or seek operations.";
        f.howToFix = "Ensure the HAL and audio service handle rapid start/pause/flush/stop state transitions without wedging the PCM driver or disabling timestamp reporting.";
        addOrUpdateFinding(f, streamDesc, detail);
    };

    mDataCallback->reset();
    Result res = mStream->start();
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: initial start() returned %s\n", convertToText(res)));
        recordCycleControlFinding("FAIL (start failed)",
                                  formatString("%s %s (%s): initial start() returned %s.",
                                               dirStr, shareStr, modeStr, convertToText(res)));
        recordFail();
        return;
    }

    bool subtestFailed = false;
    bool hasCycleJumpFailure = false;
    int64_t cumulativeTsDrift = 0;
    int64_t cumulativeHwDrift = 0;
    int32_t fullCapForwardJumpCount = 0;
    int32_t backwardResetCount = 0;
    int32_t unalignedRemJumpCount = 0;
    int64_t exampleTsError = 0;
    int64_t exampleHwError = 0;
    int64_t exampleRingOffset = 0;

    for (int32_t cycle = 1; cycle <= kRapidCycleCount && mThreadEnabled.load(); ++cycle) {
        if (!runUntilUnalignedPosition(220, capacity, burst)) {
            break;
        }

        TimestampSample preSample{};
        Result lastTsRes = Result::OK;
        if (!waitForFreshTimestamp(0, 500, &preSample, &lastTsRes)) {
            if (!mThreadEnabled.load()) break;
            appendTelemetryLine(formatString("    C%d [FAIL]: getTimestamp failed pre-%s (%s)",
                                             cycle, usePauseFlush ? "pause" : "stop",
                                             convertToText(lastTsRes)));
            subtestFailed = true;
            recordCycleControlFinding(
                    "FAIL (getTimestamp error)",
                    formatString("%s %s (%s) C%d: getTimestamp() failed before %s (%s).",
                                 dirStr, shareStr, modeStr, cycle,
                                 usePauseFlush ? "pause" : "stop", convertToText(lastTsRes)));
            break;
        }

        const int64_t preHw = (direction == Direction::Output)
                ? preSample.framesRead : preSample.framesWritten;
        const int64_t preRem = (capacity > 0) ? (preHw % capacity) : 0;
        const int64_t preComp = (capacity > 0) ? (capacity - preRem) : 0;

        if (usePauseFlush) {
            res = mStream->pause();
            if (res == Result::OK) res = mStream->flush();
        } else {
            res = mStream->stop();
        }
        if (res != Result::OK) {
            appendTelemetryLine(formatString("    C%d [FAIL]: %s returned %s",
                                             cycle, usePauseFlush ? "pause/flush" : "stop",
                                             convertToText(res)));
            subtestFailed = true;
            recordCycleControlFinding(
                    "FAIL (stop/flush error)",
                    formatString("%s %s (%s) C%d: %s returned %s.",
                                 dirStr, shareStr, modeStr, cycle,
                                 usePauseFlush ? "pause/flush" : "stop", convertToText(res)));
            break;
        }

        const int64_t restartCallNs = AudioClock::getNanoseconds();
        res = mStream->start();
        if (res != Result::OK) {
            appendTelemetryLine(formatString("    C%d [FAIL]: start() returned %s",
                                             cycle, convertToText(res)));
            subtestFailed = true;
            recordCycleControlFinding(
                    "FAIL (start error)",
                    formatString("%s %s (%s) C%d: start() returned %s.",
                                 dirStr, shareStr, modeStr, cycle, convertToText(res)));
            break;
        }

        if (!sleepMillisInterruptible(80)) {
            break;
        }

        TimestampSample postSample{};
        if (!waitForFreshTimestamp(restartCallNs, 800, &postSample, &lastTsRes)) {
            if (!mThreadEnabled.load()) break;
            appendTelemetryLine(formatString("    C%d [FAIL]: no fresh timestamp post-start (%s)",
                                             cycle, convertToText(lastTsRes)));
            subtestFailed = true;
            recordCycleControlFinding(
                    "FAIL (no post-start ts)",
                    formatString("%s %s (%s) C%d: no fresh timestamp within 800 ms after restart (%s).",
                                 dirStr, shareStr, modeStr, cycle, convertToText(lastTsRes)));
            break;
        }

        const int64_t postHw = (direction == Direction::Output)
                ? postSample.framesRead : postSample.framesWritten;
        const int64_t dWritten = postSample.framesWritten - preSample.framesWritten;
        const int64_t dRead = postSample.framesRead - preSample.framesRead;
        const int64_t dTsPos = postSample.tsPosition - preSample.tsPosition;
        const int64_t dApp = (direction == Direction::Output) ? dWritten : dRead;
        const int64_t dHw = (direction == Direction::Output) ? dRead : dWritten;

        const int64_t tsError = dTsPos - dApp;
        const int64_t dmaError = dHw - dApp;
        cumulativeTsDrift += tsError;
        cumulativeHwDrift += dmaError;

        const int64_t jumpThreshold = isShared
                ? std::max(capacity, burst * 12)
                : std::max(capacity / 2, burst * 6);
        const bool backwardTs = (postSample.tsPosition < preSample.tsPosition);
        const bool backwardHw = (postHw < preHw);
        const bool causalViolationOut = (direction == Direction::Output) &&
                ((postSample.tsPosition > postSample.framesWritten + burst) ||
                 (postSample.framesRead > postSample.framesWritten + burst));
        const bool largeJump = (std::abs(tsError) > jumpThreshold) ||
                (std::abs(dmaError) > jumpThreshold);

        const bool cycleFail = backwardTs || backwardHw || causalViolationOut || largeJump;
        appendTelemetryLine(formatString(
                "    C%d [%s] @%-4lldf: ts=%+lldf(%+.0fms) dma=%+lldf",
                cycle,
                cycleFail ? "FAIL" : "OK  ",
                static_cast<long long>(preRem),
                static_cast<long long>(tsError),
                framesToMs(tsError, sampleRate),
                static_cast<long long>(dmaError)));

        if (cycleFail) {
            subtestFailed = true;
            hasCycleJumpFailure = true;
            exampleTsError = tsError;
            exampleHwError = dmaError;
            exampleRingOffset = preRem;

            if (tsError > 0 && std::abs(tsError - capacity) <= burst * 6) {
                fullCapForwardJumpCount++;
            } else if (backwardTs || backwardHw ||
                       std::abs(tsError + preComp) <= burst * 3 ||
                       std::abs(dmaError + preComp) <= burst * 3) {
                backwardResetCount++;
            } else if (std::abs(tsError - preRem) <= burst * 3 ||
                       std::abs(dmaError - preRem) <= burst * 3) {
                unalignedRemJumpCount++;
            } else {
                backwardResetCount++;
            }
        }
    }

    if (!mThreadEnabled.load()) {
        return;
    }

    if (subtestFailed) {
        *inoutAnyFailed = true;
        appendTelemetryLine(formatString(
                "    Net Drift : ts=%+lldf (%+.1fms), dma=%+lldf (%+.1fms)",
                static_cast<long long>(cumulativeTsDrift),
                framesToMs(cumulativeTsDrift, sampleRate),
                static_cast<long long>(cumulativeHwDrift),
                framesToMs(cumulativeHwDrift, sampleRate)));
        appendTelemetryLine("    Result    : FAIL\n");

        if (hasCycleJumpFailure) {
            if (fullCapForwardJumpCount > 0 && direction == Direction::Input) {
                *inoutFailureTag = formatString("FAIL (%+.0fms jump/stop)",
                                                framesToMs(exampleTsError, sampleRate));
                Finding f;
                f.id = "CYCLE-INPUT-BUFFER-CAPACITY-JUMP";
                f.severity = "FAIL";
                f.title = "Input timestamp jumps forward by +1 buffer capacity on every Stop -> Start";
                f.whatHappened = "When the input MMAP stream was stopped at an unaligned ring-buffer offset (rem = dma % bufferCapacity) and restarted, getTimestamp().position and getFramesWritten() jumped forward by +1 full buffer capacity relative to the frames actually read by the app.";
                f.androidContract = "StreamDescriptor.aidl & AAudio.h: Across stop() -> start(), hardware and presentation frame counters must advance only by the frames actually captured (~0-2 bursts) without adding full ring-buffer multiples.";
                f.whyItMatters = "Every time an input stream is stopped and restarted, capture timestamps jump forward by a full buffer capacity per restart, breaking Acoustic Echo Cancellation (AEC) and multi-track recording alignment.";
                f.howToFix = "1. Check how the HAL and audio service coordinate cumulative MMAP frame positions across stop() -> start().\n  2. If the HAL rounds its pre-stop cumulative position up to the next full ring-buffer boundary (+bufferCapacity) on stop() while failing to stop the underlying capture PCM driver on pause/stop, the hardware DMA pointer continues from its previous ring offset (rem) on restart, adding +1 full bufferCapacity on every stop/start cycle.\n  3. Ensure the Input MMAP HAL stops the capture PCM driver on pause/stop and keeps cumulative position accounting consistent with the hardware ring pointer reset.";
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Jumped forward by %+lld frames (%+.1f ms = %+.2fx bufferCapacity) per stop/start; net drift over %d cycles = %+lld frames (%+.1f ms).",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(exampleTsError),
                                                framesToMs(exampleTsError, sampleRate),
                                                static_cast<double>(exampleTsError) / capacity,
                                                kRapidCycleCount,
                                                static_cast<long long>(cumulativeTsDrift),
                                                framesToMs(cumulativeTsDrift, sampleRate)));
            } else if (unalignedRemJumpCount > 0) {
                *inoutFailureTag = "FAIL (ring offset jump)";
                Finding f;
                f.id = "CYCLE-UNALIGNED-RING-OFFSET-JUMP";
                f.severity = "FAIL";
                f.title = "MMAP frame counter jumps by unaligned ring-buffer offset across Stop/Flush";
                f.whatHappened = "Stopping or flushing the MMAP stream at an unaligned ring-buffer offset (rem = dma % bufferCapacity) and restarting caused the reported frame position to jump by the unaligned ring offset.";
                f.androidContract = "StreamDescriptor.aidl: MMAP hardware and observable frame counters must remain continuous without jumping by (dma % bufferCapacity) across flush or stop.";
                f.whyItMatters = "Causes cumulative timestamp drift and false XRun buffer expansion on every stream flush or restart.";
                f.howToFix = "When the hardware DMA ring buffer resets to index 0 on stop/flush, align the cumulative pre-stop frame offset to bufferCapacity boundaries rather than latching an unaligned remainder.";
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Jumped by tsError=%+lldf (%+.1f ms), dmaError=%+lldf (%+.1f ms) at ringOffset=%lldf.",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(exampleTsError),
                                                framesToMs(exampleTsError, sampleRate),
                                                static_cast<long long>(exampleHwError),
                                                framesToMs(exampleHwError, sampleRate),
                                                static_cast<long long>(exampleRingOffset)));
            } else {
                *inoutFailureTag = formatString("FAIL (ts jump %+.0fms)",
                                                framesToMs(exampleTsError, sampleRate));
                Finding f;
                f.id = "CYCLE-POSITION-RESET-OR-JUMP";
                f.severity = "FAIL";
                f.title = "MMAP frame counter resets or jumps across rapid Start / Stop / Flush cycles";
                f.whatHappened = "Across rapid pause -> flush -> start or stop -> start transitions, the reported timestamp or DMA counter reset or jumped instead of continuing smoothly from its pre-stop frame count.";
                f.androidContract = "StreamDescriptor.aidl: For MMAP streams, Reply.hardware and Reply.observable frame counters must continue monotonically without resetting across start->stop->start or pause->flush->start transitions.";
                f.whyItMatters = "Causes A/V lip-sync jumps and corrupts buffer occupancy tracking after pause, seek, or stop.";
                f.howToFix = "1. Do not zero the cumulative MMAP frame counter on stop(), pause(), or flush(); accumulate total frames transferred across restarts.\n  2. When latching the pre-stop position while the PCM stream is stopped, keep the latch active across all status queries (both Reply.observable and Reply.hardware) until start() resumes the hardware stream.";
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Jumped by tsError=%+lldf (%+.1f ms), dmaError=%+lldf (%+.1f ms); net drift = %+lldf (%+.1f ms).",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(exampleTsError),
                                                framesToMs(exampleTsError, sampleRate),
                                                static_cast<long long>(exampleHwError),
                                                framesToMs(exampleHwError, sampleRate),
                                                static_cast<long long>(cumulativeTsDrift),
                                                framesToMs(cumulativeTsDrift, sampleRate)));
            }
        }
        recordFail();
    } else {
        appendTelemetryLine(formatString(
                "    Net Drift : ts=%+lldf (%+.1fms), dma=%+lldf (%+.1fms)",
                static_cast<long long>(cumulativeTsDrift),
                framesToMs(cumulativeTsDrift, sampleRate),
                static_cast<long long>(cumulativeHwDrift),
                framesToMs(cumulativeHwDrift, sampleRate)));
        appendTelemetryLine("    Result    : PASS\n");
        recordPass();
    }
}

void TestMmapTimestamp::runStandbyResumeCheck(Direction direction,
                                              bool usePauseFlush,
                                              size_t /* rowIndex */,
                                              bool *inoutAnyFailed,
                                              std::string *inoutFailureTag) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const bool isExclusive = (mStream->getSharingMode() == SharingMode::Exclusive);
    const char *shareStr = isExclusive ? "EXCLUSIVE" : "SHARED";
    const char *modeStr = usePauseFlush
            ? "Pause -> 3.5s Standby -> Flush -> Start"
            : "Stop -> 3.5s Standby -> Start";
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t burst = mStream->getFramesPerBurst();
    const double capMs = framesToMs(capacity, sampleRate);
    const std::string streamDesc = formatString("%s %s (%d Hz, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate, capacity, capMs, burst);

    appendTelemetryLine(formatString("  [Phase 3] %s", modeStr));

    auto recordStandbySetupFinding = [&](const std::string &tag, const std::string &detail) {
        *inoutAnyFailed = true;
        *inoutFailureTag = tag;
        Finding f;
        f.id = "STANDBY-STREAM-CONTROL-FAILURE";
        f.severity = "FAIL";
        f.title = "MMAP stream control call failed during >3s idle standby check";
        f.whatHappened = "Starting, pausing, stopping, or flushing the MMAP stream before or after the 3.5s idle standby window returned an error.";
        f.androidContract = "AAudio.h: Stream state transitions across idle standby (>3s stopped/paused) must succeed without returning error codes.";
        f.whyItMatters = "Preventing stream restart after idle standby breaks audio playback and capture whenever a stream is idle for a few seconds.";
        f.howToFix = "Ensure the HAL and audio service cleanly close and recreate the MMAP shared buffer across standby entry and exit without leaking resources or returning errors.";
        addOrUpdateFinding(f, streamDesc, detail);
    };

    mDataCallback->reset();
    Result res = mStream->start();
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: initial start() returned %s\n", convertToText(res)));
        recordStandbySetupFinding("FAIL (start failed)",
                                  formatString("%s %s (%s): initial start() returned %s.",
                                               dirStr, shareStr, modeStr, convertToText(res)));
        recordFail();
        return;
    }

    if (!runUntilUnalignedPosition(300, capacity, burst)) {
        return;
    }

    TimestampSample preSample{};
    Result lastTsRes = Result::OK;
    if (!waitForFreshTimestamp(0, 500, &preSample, &lastTsRes)) {
        if (!mThreadEnabled.load()) return;
        appendTelemetryLine(formatString("    FAIL: no valid timestamp before standby (%s)\n",
                                         convertToText(lastTsRes)));
        recordStandbySetupFinding("FAIL (pre-standby ts error)",
                                  formatString("%s %s (%s): no valid timestamp before standby (%s).",
                                               dirStr, shareStr, modeStr, convertToText(lastTsRes)));
        recordFail();
        return;
    }

    const int32_t xRunsBefore = mStream->getXRunCount().value();
    const int64_t preHw = (direction == Direction::Output)
            ? preSample.framesRead : preSample.framesWritten;
    const int64_t preApp = (direction == Direction::Output)
            ? preSample.framesWritten : preSample.framesRead;
    const int64_t preRem = (capacity > 0) ? (preHw % capacity) : 0;

    if (usePauseFlush) {
        res = mStream->pause();
    } else {
        res = mStream->stop();
    }
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: %s returned %s\n",
                                         usePauseFlush ? "pause()" : "stop()",
                                         convertToText(res)));
        recordStandbySetupFinding("FAIL (stop/pause error)",
                                  formatString("%s %s (%s): %s returned %s.",
                                               dirStr, shareStr, modeStr,
                                               usePauseFlush ? "pause()" : "stop()",
                                               convertToText(res)));
        recordFail();
        return;
    }

    const int64_t cbBeforeStandby = mDataCallback->getTotalCallbackFrames();

    appendTelemetryLine(formatString(
            "    Before : app=%lldf dma=%lldf ts=%lldf (@%lldf)",
            static_cast<long long>(preApp),
            static_cast<long long>(preHw),
            static_cast<long long>(preSample.tsPosition),
            static_cast<long long>(preRem)));
    appendTelemetryLine(formatString(
            "    Sleep  : Waiting %d ms for MMAP standby...",
            kStandbySleepMs));

    if (!sleepMillisInterruptible(kStandbySleepMs)) {
        return;
    }

    if (usePauseFlush) {
        res = mStream->flush();
        if (res != Result::OK) {
            appendTelemetryLine(formatString("    FAIL: flush() after standby returned %s\n",
                                             convertToText(res)));
            recordStandbySetupFinding("FAIL (flush error)",
                                      formatString("%s %s (%s): flush() after standby returned %s.",
                                                   dirStr, shareStr, modeStr, convertToText(res)));
            recordFail();
            return;
        }
    }

    const int64_t restartCallNs = AudioClock::getNanoseconds();
    res = mStream->start();
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: start() after standby returned %s\n",
                                         convertToText(res)));
        recordStandbySetupFinding("FAIL (post-standby start)",
                                  formatString("%s %s (%s): start() after standby returned %s.",
                                               dirStr, shareStr, modeStr, convertToText(res)));
        recordFail();
        return;
    }

    if (!sleepMillisInterruptible(200)) {
        return;
    }

    TimestampSample postSample{};
    if (!waitForFreshTimestamp(restartCallNs, 1000, &postSample, &lastTsRes)) {
        if (!mThreadEnabled.load()) return;
        const int64_t postAppNow = (direction == Direction::Output)
                ? mStream->getFramesWritten() : mStream->getFramesRead();
        const int64_t postHwNow = (direction == Direction::Output)
                ? mStream->getFramesRead() : mStream->getFramesWritten();
        const int64_t cbAfterNow = mDataCallback->getTotalCallbackFrames();
        const int64_t cbDeltaNow = cbAfterNow - cbBeforeStandby;
        appendTelemetryLine(formatString(
                "    After  : app=%lldf dma=%lldf (cb=%+lldf)",
                static_cast<long long>(postAppNow),
                static_cast<long long>(postHwNow),
                static_cast<long long>(cbDeltaNow)));
        appendTelemetryLine(formatString(
                "    Error  : getTimestamp()=%s",
                convertToText(lastTsRes)));
        appendTelemetryLine("    Result : FAIL\n");

        *inoutAnyFailed = true;
        if (cbDeltaNow == 0) {
            *inoutFailureTag = "FAIL (0 callbacks on resume)";
            Finding fStall;
            fStall.id = (direction == Direction::Input)
                    ? "STANDBY-INPUT-ZERO-CALLBACKS"
                    : "STANDBY-OUTPUT-ZERO-CALLBACKS";
            fStall.severity = "FAIL";
            fStall.title = (direction == Direction::Input)
                    ? "Input MMAP stream delivers 0 audio callbacks after resuming from >3s idle standby"
                    : "Output MMAP stream delivers 0 audio callbacks after resuming from >3s idle standby";
            fStall.whatHappened = (direction == Direction::Input)
                    ? "After stopping the input MMAP stream, waiting 3.5 seconds for MMAP idle standby to close the hardware buffer, and calling start(), start() returned OK but onAudioReady() delivered 0 callbacks and capture DMA never advanced."
                    : "After pausing or stopping the output MMAP stream, waiting 3.5 seconds for MMAP idle standby to close the hardware buffer, and calling start(), start() returned OK but onAudioReady() delivered 0 callbacks and playback DMA never advanced.";
            fStall.androidContract = "StreamDescriptor.aidl & AAudio.h: After an MMAP stream enters idle standby (>3s stopped) and is restarted via AAudioStream_requestStart(), hardware DMA and data callbacks must resume delivering frames.";
            fStall.whyItMatters = "Any audio app using an MMAP stream (Exclusive or Shared) that pauses or stops for >3 seconds and restarts it receives 0 audio callbacks permanently (silent playback or frozen recording after standby).";
            fStall.howToFix = "1. When exiting MMAP idle standby, the audio service allocates the MMAP buffer (sending the HAL 'start' command transitioning STANDBY -> IDLE) before reconnecting the audio patch.\n  2. If the HAL attempts to start the PCM driver during STANDBY -> IDLE before the audio route/backend DAI is enabled, the driver start fails. If the subsequent 'burst' command (IDLE -> ACTIVE) returns early on MMAP streams without starting the PCM driver once the route is active, DMA never starts.\n  3. Ensure the HAL starts (or retries starting) the MMAP PCM stream when transitioning to ACTIVE on 'burst' once the audio route is connected.";
            addOrUpdateFinding(fStall, streamDesc,
                               formatString("%s %s (%s): After 3.5s standby and 1200 ms post-start wait, callback received cbDelta=+0 frames and appFrames remained stuck at %lldf.",
                                            dirStr, shareStr, modeStr,
                                            static_cast<long long>(postAppNow)));
        } else {
            *inoutFailureTag = formatString("FAIL (ts %s)", convertToText(lastTsRes));
            Finding f;
            f.id = (direction == Direction::Input)
                    ? "STANDBY-INPUT-TIMESTAMP-INVALID-STATE"
                    : "STANDBY-OUTPUT-TIMESTAMP-INVALID-STATE";
            f.severity = "FAIL";
            f.title = (direction == Direction::Input)
                    ? "Input getTimestamp() fails after resuming from >3s idle standby"
                    : "Output getTimestamp() fails after resuming from >3s idle standby";
            f.whatHappened = "After 3.5 seconds of idle standby and start(), audio callbacks resumed transferring frames, but the application frame counter stayed frozen at its pre-standby value and getTimestamp() failed rather than returning updated timestamps.";
            f.androidContract = "AAudio.h (AAudioStream_getTimestamp & AAudioStream_getFramesRead/Written): Must continue returning valid monotonic timestamps and frame counts after a stopped stream resumes from MMAP idle standby (>3s idle).";
            f.whyItMatters = "Audio apps that pause or stop an MMAP stream for >3 seconds and restart it lose timestamp reporting and see frozen frame counters until post-standby transfer exceeds the pre-standby frame total.";
            f.howToFix = "When reallocating the client shared memory FIFO on MMAP standby exit (which resets the new FIFO's read/write counters to 0), preserve and carry over the pre-standby transferred frame count into the client's frame offset so getFramesRead(), getFramesWritten(), and getTimestamp() remain continuous across standby.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s standby, callback transferred %+lld new frames (%.1f ms), but appFrames stayed clamped at %lldf and getTimestamp() failed for 1000 ms with %s (%d).",
                                            dirStr, shareStr, modeStr,
                                            static_cast<long long>(cbDeltaNow),
                                            framesToMs(cbDeltaNow, sampleRate),
                                            static_cast<long long>(postAppNow),
                                            convertToText(lastTsRes),
                                            static_cast<int>(lastTsRes)));
        }
        recordFail();
        return;
    }

    const int32_t xRunsAfter = mStream->getXRunCount().value();
    const int32_t dXRuns = xRunsAfter - xRunsBefore;
    const int64_t postHw = (direction == Direction::Output)
            ? postSample.framesRead : postSample.framesWritten;
    const int64_t postApp = (direction == Direction::Output)
            ? postSample.framesWritten : postSample.framesRead;
    const int64_t dWritten = postSample.framesWritten - preSample.framesWritten;
    const int64_t dRead = postSample.framesRead - preSample.framesRead;
    const int64_t dTsPos = postSample.tsPosition - preSample.tsPosition;
    const int64_t dCb = postSample.callbackFrames - cbBeforeStandby;
    const int64_t dApp = (direction == Direction::Output) ? dWritten : dRead;
    const int64_t dHw = (direction == Direction::Output) ? dRead : dWritten;
    const bool appCounterFrozen = (dCb > capacity && dApp <= burst * 2);
    const bool callbackStalled = (dCb == 0);
    const int64_t referenceAppDelta = (appCounterFrozen && dCb > 0) ? dCb : dApp;
    const int64_t tsError = dTsPos - referenceAppDelta;
    const int64_t dmaError = dHw - referenceAppDelta;

    const int64_t jumpThreshold = std::max(capacity / 2, burst * 6);
    const bool backwardTs = (postSample.tsPosition < preSample.tsPosition);
    const bool backwardHw = (postHw < preHw);
    const bool causalViolationOut = (direction == Direction::Output) &&
            ((postSample.tsPosition > postSample.framesWritten + burst) ||
             (postSample.framesRead > postSample.framesWritten + burst));
    const bool largeJump = (std::abs(tsError) > jumpThreshold) ||
            (std::abs(dmaError) > jumpThreshold);
    const bool checkFailed = callbackStalled || appCounterFrozen || backwardTs ||
            backwardHw || causalViolationOut || largeJump;

    appendTelemetryLine(formatString(
            "    After  : app=%lldf dma=%lldf ts=%lldf (cb=%+lldf)",
            static_cast<long long>(postApp),
            static_cast<long long>(postHw),
            static_cast<long long>(postSample.tsPosition),
            static_cast<long long>(dCb)));
    appendTelemetryLine(formatString(
            "    Jump   : tsErr=%+lldf(%+.0fms) dmaEr=%+lldf(%+.0fms)",
            static_cast<long long>(tsError),
            framesToMs(tsError, sampleRate),
            static_cast<long long>(dmaError),
            framesToMs(dmaError, sampleRate)));
    appendTelemetryLine(formatString(
            "    Flags  : appFrozen=%s, cbStall=%s, xRuns=%d",
            appCounterFrozen ? "YES" : "NO",
            callbackStalled ? "YES" : "NO",
            dXRuns));
    appendTelemetryLine(formatString(
            "    Result : %s\n",
            checkFailed ? "FAIL" : "PASS"));

    if (checkFailed) {
        *inoutAnyFailed = true;
        if (callbackStalled) {
            *inoutFailureTag = "FAIL (0 callbacks on resume)";
            Finding f;
            f.id = (direction == Direction::Input)
                    ? "STANDBY-INPUT-ZERO-CALLBACKS"
                    : "STANDBY-OUTPUT-ZERO-CALLBACKS";
            f.severity = "FAIL";
            f.title = (direction == Direction::Input)
                    ? "Input MMAP stream delivers 0 audio callbacks after resuming from >3s idle standby"
                    : "Output MMAP stream delivers 0 audio callbacks after resuming from >3s idle standby";
            f.whatHappened = (direction == Direction::Input)
                    ? "After stopping the input MMAP stream, waiting 3.5 seconds for MMAP idle standby to close the hardware buffer, and calling start(), start() returned OK but onAudioReady() delivered 0 callbacks and capture DMA never advanced."
                    : "After pausing or stopping the output MMAP stream, waiting 3.5 seconds for MMAP idle standby to close the hardware buffer, and calling start(), start() returned OK but onAudioReady() delivered 0 callbacks and playback DMA never advanced.";
            f.androidContract = "StreamDescriptor.aidl & AAudio.h: After an MMAP stream enters idle standby (>3s stopped) and is restarted via AAudioStream_requestStart(), hardware DMA and data callbacks must resume delivering frames.";
            f.whyItMatters = "Any audio app using an MMAP stream (Exclusive or Shared) that pauses or stops for >3 seconds and restarts it receives 0 audio callbacks permanently (silent playback or frozen recording after standby).";
            f.howToFix = "1. When exiting MMAP idle standby, the audio service allocates the MMAP buffer (sending the HAL 'start' command transitioning STANDBY -> IDLE) before reconnecting the audio patch.\n  2. If the HAL attempts to start the PCM driver during STANDBY -> IDLE before the audio route/backend DAI is enabled, the driver start fails. If the subsequent 'burst' command (IDLE -> ACTIVE) returns early on MMAP streams without starting the PCM driver once the route is active, DMA never starts.\n  3. Ensure the HAL starts (or retries starting) the MMAP PCM stream when transitioning to ACTIVE on 'burst' once the audio route is connected.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s standby and 200+ ms post-start wait, callback received cbDelta=+0 frames and app/DMA frame counters remained stuck at %lldf.",
                                            dirStr, shareStr, modeStr,
                                            static_cast<long long>(postApp)));
        } else if (appCounterFrozen || backwardTs || tsError < -jumpThreshold) {
            if (appCounterFrozen) {
                *inoutFailureTag = formatString("FAIL (app frozen, ts %+.0fms)",
                                                framesToMs(tsError, sampleRate));
            } else {
                *inoutFailureTag = formatString("FAIL (ts jumped %+.0fms)",
                                                framesToMs(tsError, sampleRate));
            }

            Finding f;
            f.id = (direction == Direction::Output)
                    ? "STANDBY-OUTPUT-POSITION-REGRESSION"
                    : "STANDBY-INPUT-POSITION-REGRESSION";
            f.severity = "FAIL";
            f.title = (direction == Direction::Output)
                    ? "Output frame counters freeze or jump backward after resuming from >3s idle standby"
                    : "Input frame counters freeze or jump backward after resuming from >3s idle standby";
            f.whatHappened = "After pausing or stopping an MMAP stream for 3.5 seconds (triggering MMAP idle standby) and calling start(), audio callbacks resumed transferring frames, but getFramesWritten()/getFramesRead() froze at their pre-standby value or getTimestamp().position jumped backward toward 0.";
            f.androidContract = "AAudio.h & StreamDescriptor.aidl: Exiting MMAP idle standby (>3s stopped) must preserve monotonic getFramesWritten(), getFramesRead(), and getTimestamp().position without resetting to 0 or shifting by buffer multiples.";
            f.whyItMatters = "When an MMAP stream is paused or stopped for >3 seconds and resumed, getFramesWritten()/getFramesRead() freeze until post-standby transfer catches up and getTimestamp().position jumps backward by hundreds of milliseconds, breaking A/V lip-sync and recording alignment.";
            f.howToFix = "1. When recreating the MMAP client FIFO during standby exit (which resets the new FIFO's counters to 0), carry over the pre-standby frame count into the client's frame offset so client and timestamp counters continue from the pre-standby total.\n  2. In the HAL, keep the pre-stop position latched for both Reply.observable and Reply.hardware while stopped/paused (clearing the latch in start() rather than on the first position read) so hardware and presentation offsets stay synchronized across standby.";
            if (appCounterFrozen) {
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Callback transferred %+lld new frames (%.1f ms) after standby, but getFramesWritten()/getFramesRead() stayed frozen at %lldf and getTimestamp().position regressed from %lldf to %lldf (tsError=%+lldf / %+.1f ms).",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(dCb),
                                                framesToMs(dCb, sampleRate),
                                                static_cast<long long>(postApp),
                                                static_cast<long long>(preSample.tsPosition),
                                                static_cast<long long>(postSample.tsPosition),
                                                static_cast<long long>(tsError),
                                                framesToMs(tsError, sampleRate)));
            } else {
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): After 3.5s standby, getTimestamp().position jumped BACKWARD relative to appFrames by %+lld frames (%+.1f ms), from %lldf to %lldf while appFrames advanced %lldf -> %lldf.",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(tsError),
                                                framesToMs(tsError, sampleRate),
                                                static_cast<long long>(preSample.tsPosition),
                                                static_cast<long long>(postSample.tsPosition),
                                                static_cast<long long>(preApp),
                                                static_cast<long long>(postApp)));
            }
        } else {
            *inoutFailureTag = formatString("FAIL (ts jump %+.0fms)",
                                            framesToMs(tsError, sampleRate));
            Finding f;
            f.id = "STANDBY-UNALIGNED-RING-OFFSET";
            f.severity = "FAIL";
            f.title = "MMAP frame position jumps by unaligned ring-buffer offset after >3s idle standby";
            f.whatHappened = "After resuming from 3.5 seconds of idle standby, the reported frame position jumped by the unaligned ring-buffer stop offset (rem = dma % bufferCapacity).";
            f.androidContract = "StreamDescriptor.aidl: Exiting standby on an MMAP stream must maintain monotonic hardware and observable frame counts without adding unaligned ring-buffer remainders.";
            f.whyItMatters = "Causes forward timestamp jumps and false XRun buffer inflation whenever an audio stream resumes after >3s idle.";
            f.howToFix = "When entering HAL standby on an MMAP stream, align the saved standby position to bufferCapacity boundaries to match the reset of the hardware ring buffer to index 0.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s standby (stop@ringOffset=%lld/%df), jumped by tsError=%+lldf (%+.1f ms), dmaError=%+lldf (%+.1f ms).",
                                            dirStr, shareStr, modeStr,
                                            static_cast<long long>(preRem),
                                            capacity,
                                            static_cast<long long>(tsError),
                                            framesToMs(tsError, sampleRate),
                                            static_cast<long long>(dmaError),
                                            framesToMs(dmaError, sampleRate)));
        }
        recordFail();
    } else {
        recordPass();
    }
}

