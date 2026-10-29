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
#include <aaudio/AAudioLoader.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unistd.h>

#include "common/OboeDebug.h"
#include "oboe/AudioClock.h"
#include "OboeTools.h"

using namespace oboe;

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi = 3.14159265358979323846f;
constexpr int64_t kStateChangeTimeoutNs = 500LL * NANOS_PER_MILLISECOND;

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

std::string formatSampleRateKHz(int32_t sampleRate) {
    if (sampleRate % 1000 == 0) {
        return formatString("%dkHz", sampleRate / 1000);
    }
    return formatString("%.1fkHz", static_cast<double>(sampleRate) / 1000.0);
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
    if (audioStream->getDirection() == Direction::Output && audioData != nullptr) {
        const int32_t channelCount = std::max(1, audioStream->getChannelCount());
        const int32_t sampleRate = std::max(1, audioStream->getSampleRate());
        const float phaseIncrement = kTwoPi * 440.0f / static_cast<float>(sampleRate);
        if (audioStream->getFormat() == AudioFormat::Float) {
            float *floatData = static_cast<float *>(audioData);
            for (int32_t i = 0; i < numFrames; i++) {
                const float sample = std::sin(mPhase) * 0.05f;
                for (int32_t ch = 0; ch < channelCount; ch++) {
                    *floatData++ = sample;
                }
                mPhase += phaseIncrement;
                if (mPhase >= kPi) {
                    mPhase -= kTwoPi;
                }
            }
        } else {
            const int32_t bytesPerFrame = audioStream->getBytesPerFrame();
            if (bytesPerFrame > 0 && numFrames > 0) {
                std::memset(audioData, 0, static_cast<size_t>(bytesPerFrame) * numFrames);
            }
        }
    }
    return DataCallbackResult::Continue;
}

int32_t TestMmapTimestamp::start(const ConfigFlags &flags) {
    std::lock_guard<std::mutex> lifecycleLock(mLifecycleMutex);
    if (mIsRunning.load()) {
        return -1;
    }
    mThreadEnabled.store(false);
    if (mWorkerThread.joinable()) {
        mWorkerThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(mReportMutex);
        mTelemetryLog.clear();
        mCurrentProgress = "Initializing MMAP diagnostic suite...";
        mScorecard.clear();
        mFindings.clear();
        mHasRun = true;
        mSuiteCompleted = false;
        mSuiteStoppedByUser = false;
        mWorkaroundsDisabledDuringRun = true;
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

int32_t TestMmapTimestamp::requestStop() {
    mThreadEnabled.store(false);
    return 0;
}

int32_t TestMmapTimestamp::stop() {
    std::lock_guard<std::mutex> lifecycleLock(mLifecycleMutex);
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
    std::string overallStr = mHasRun ? "RUNNING..." : "NOT STARTED";
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
        } else if (totalChecks == 0) {
            overallStr = "SKIPPED (0 checks executed)";
        } else {
            overallStr = "PASS";
        }
    }

    const bool workaroundsOff = mWorkaroundsDisabledDuringRun || !OboeGlobals::areWorkaroundsEnabled();

    out << "================================================\n";
    out << "1. SUMMARY & SCORECARD\n";
    out << "================================================\n";
    out << formatString("Overall Verdict : %s\n", overallStr.c_str());
    if (!suiteCompleted) {
        out << formatString("Current Status  : %s\n", mCurrentProgress.c_str());
    }
    out << formatString("Checks Executed : %d (%d PASS, %d WARN, %d FAIL)\n",
                        totalChecks, mPassCount, mWarnCount, mFailCount);
    out << formatString("MMAP Support    : MMap=%s | Excl=%s | Workarounds=%s\n\n",
                        AAudioExtensions::getInstance().isMMapSupported() ? "YES" : "NO",
                        AAudioExtensions::getInstance().isMMapExclusiveSupported() ? "YES" : "NO",
                        workaroundsOff ? "OFF" : "ON");

    out << "SCORECARD BY STREAM:\n";
    for (const auto &row : mScorecard) {
        out << formatString("* %s (%s)\n",
                            row.streamLabel.c_str(),
                            row.configSummary.c_str());
        out << formatString("    [1] Steady-State & DMA : %s\n", row.steadyStatus.c_str());
        out << formatString("    [2] Rapid Stop/Start   : %s\n", row.rapidCyclesStatus.c_str());
        out << formatString("    [3] %-18s : %s\n",
                            row.isShared ? "3.5s Idle Resume" : "3.5s Idle Standby",
                            row.standbyStatus.c_str());
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
            out << "[ISSUE #" << (i + 1) << "] [" << f.id << "] " << f.title << "\n";
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
    out << "- app  : Client stream counter (getFramesWritten Out / getFramesRead In)\n";
    out << "- dma  : Opposite-end counter (AAudio IsochronousClockModel estimate seeded by HAL/service)\n";
    out << "- ts   : Presentation timestamp (getTimestamp: HAL Reply.observable, service fallback, or SHARED mixer)\n";
    out << "- @off : Ring-buffer offset at transition ((dma - base) % capacity)\n";
    out << "- tsErr: Delta between projected ts progress and client stream progress across transition\n";
    out << "- dmaEr: Delta between dma progress and client stream progress across transition\n\n";
    out << mTelemetryLog;

    return out.str();
}

bool TestMmapTimestamp::sleepMillisInterruptible(int32_t durationMs) {
    constexpr int32_t kStepMs = 20;
    int32_t remainingMs = durationMs;
    while (remainingMs > 0 && mThreadEnabled.load()) {
        if (mErrorCallback && mErrorCallback->isDisconnected()) {
            return false;
        }
        const int32_t sleepMs = std::min(kStepMs, remainingMs);
        AudioClock::sleepForNanos(static_cast<int64_t>(sleepMs) * NANOS_PER_MILLISECOND);
        remainingMs -= sleepMs;
    }
    return mThreadEnabled.load() && (!mErrorCallback || !mErrorCallback->isDisconnected());
}

void TestMmapTimestamp::runSuite(ConfigFlags flags) {
    const bool prevWorkarounds = OboeGlobals::areWorkaroundsEnabled();
    OboeGlobals::setWorkaroundsEnabled(false);

    AAudioLoader *loader = AAudioLoader::getInstance();
    const bool hasRawPolicyApi = (loader != nullptr && loader->open() == 0 &&
            loader->aaudio_getMMapPolicy != nullptr && loader->aaudio_setMMapPolicy != nullptr);
    const aaudio_policy_t prevRawPolicy = hasRawPolicyApi
            ? loader->aaudio_getMMapPolicy()
            : static_cast<aaudio_policy_t>(MMapPolicy::Unspecified);
    const bool prevMmapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    bool overrodeMmapPolicy = false;
    if (!prevMmapEnabled) {
        AAudioExtensions::getInstance().setMMapEnabled(true);
        overrodeMmapPolicy = true;
    }

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
            row.isShared = (p.sharing == SharingMode::Shared);
            row.steadyStatus = flags.runSteadyState ? "PENDING" : "SKIP";
            row.rapidCyclesStatus = flags.runRapidCycles ? "PENDING" : "SKIP";
            row.standbyStatus = flags.runStandbyResume ? "PENDING" : "SKIP";
            mScorecard.push_back(std::move(row));
        }
    }

    for (size_t i = 0; i < plans.size() && mThreadEnabled.load(); ++i) {
        runSingleConfig(plans[i].dir, plans[i].sharing, flags, i);
    }

    if (overrodeMmapPolicy) {
        if (hasRawPolicyApi) {
            loader->aaudio_setMMapPolicy(prevRawPolicy);
        } else {
            AAudioExtensions::getInstance().setMMapEnabled(prevMmapEnabled);
        }
    }
    OboeGlobals::setWorkaroundsEnabled(prevWorkarounds);

    {
        std::lock_guard<std::mutex> lock(mReportMutex);
        mWorkaroundsDisabledDuringRun = false;
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
            mResult.store(RESULT_STOPPED);
        } else {
            mCurrentProgress = "Completed.";
            if (mFailCount > 0) {
                mResult.store(RESULT_FAIL);
            } else if (mWarnCount > 0 || (mPassCount + mWarnCount + mFailCount) == 0) {
                mResult.store(RESULT_WARN);
            } else {
                mResult.store(RESULT_PASS);
            }
        }
        const std::string fullReport = buildFullReportLocked(true);
        std::istringstream iss(fullReport);
        std::string line;
        while (std::getline(iss, line)) {
            LOGI("[MmapReport] %s", line.c_str());
        }
    }

    mIsRunning.store(false);
}

Result TestMmapTimestamp::openMmapStream(Direction direction, SharingMode sharingMode) {
    closeStream();

    auto tryOpenWithChannels = [&](int32_t channelCount) -> Result {
        closeStream();
        mDataCallback = std::make_shared<MyDataCallback>();
        mErrorCallback = std::make_shared<MyErrorCallback>();

        AudioStreamBuilder builder;
        builder.setAudioApi(AudioApi::AAudio)
                ->setDirection(direction)
                ->setSharingMode(sharingMode)
                ->setPerformanceMode(PerformanceMode::LowLatency)
                ->setFormat(AudioFormat::Float)
                ->setChannelCount(channelCount)
                ->setDataCallback(mDataCallback)
                ->setErrorCallback(mErrorCallback);
        if (direction == Direction::Input) {
            builder.setInputPreset(InputPreset::Unprocessed);
        }

        Result res = builder.openStream(mStream);
        if (res == Result::OK && mStream &&
            sharingMode == SharingMode::Exclusive &&
            mStream->getSharingMode() != SharingMode::Exclusive &&
            mThreadEnabled.load()) {
            closeStream();
            if (!sleepMillisInterruptible(30)) {
                return Result::ErrorClosed;
            }
            mDataCallback = std::make_shared<MyDataCallback>();
            mErrorCallback = std::make_shared<MyErrorCallback>();
            builder.setDataCallback(mDataCallback)
                    ->setErrorCallback(mErrorCallback);
            res = builder.openStream(mStream);
        }
        return res;
    };

    Result result = Result::ErrorInternal;
    if (direction == Direction::Output) {
        result = tryOpenWithChannels(kDefaultChannelCount);
        if (result == Result::OK && mStream && mStream->getFramesPerBurst() > 0) {
            mStream->setBufferSizeInFrames(mStream->getFramesPerBurst() * 2);
        }
        return result;
    }

    // Input: try stereo first, then mono and unspecified so mono-only MMAP capture devices open cleanly.
    const int32_t inputChannelCandidates[] = {
        kDefaultChannelCount,
        ChannelCount::Mono,
        ChannelCount::Unspecified
    };
    Result firstOkResult = Result::ErrorInternal;
    int32_t firstOkChannels = kDefaultChannelCount;
    for (int32_t ch : inputChannelCandidates) {
        if (!mThreadEnabled.load()) {
            return Result::ErrorClosed;
        }
        result = tryOpenWithChannels(ch);
        if (result == Result::OK && mStream) {
            if (AAudioExtensions::getInstance().isMMapUsed(mStream.get())) {
                return Result::OK;
            }
            if (firstOkResult != Result::OK) {
                firstOkResult = Result::OK;
                firstOkChannels = ch;
            }
        }
    }
    if (firstOkResult == Result::OK && (!mStream || result != Result::OK)) {
        result = tryOpenWithChannels(firstOkChannels);
    }
    return result;
}

void TestMmapTimestamp::closeStream() {
    if (mStream) {
        if (!mErrorCallback || !mErrorCallback->isDisconnected()) {
            mStream->stop(kStateChangeTimeoutNs);
        }
        mStream->close();
        mStream.reset();
    }
}

bool TestMmapTimestamp::checkAndReportDisconnect(const std::string &streamDesc,
                                                 const char *phaseLabel,
                                                 size_t rowIndex,
                                                 int phaseIndex) {
    if (!mErrorCallback || !mErrorCallback->isDisconnected()) {
        return false;
    }
    const Result err = mErrorCallback->getError();
    appendTelemetryLine(formatString(
            "    [DISCONNECTED] %s interrupted by audio route change (%s) — rerun with stable route.",
            phaseLabel, convertToText(err)));
    appendTelemetryLine("");
    if (phaseIndex > 0) {
        setScorecardCell(rowIndex, phaseIndex, "DISCONNECTED (route changed — rerun)");
    }
    Finding f;
    f.id = "STREAM-DISCONNECTED-ROUTE-CHANGE";
    f.severity = "WARNING";
    f.title = "MMAP stream disconnected due to audio route change during test";
    f.whatHappened = "The active MMAP stream received onErrorBeforeClose (ErrorDisconnected) mid-phase because the audio routing changed while the diagnostic was running.";
    f.androidContract = "AAudio.h: Routing changes disconnect active MMAP streams via the error callback; applications reopen on the new device.";
    f.whyItMatters = "A disconnected stream stops advancing hardware/timestamp counters and must not be misdiagnosed as a HAL timestamp defect.";
    f.howToFix = "Avoid plugging/unplugging headsets, Bluetooth devices, or USB audio accessories during the run, then re-run the diagnostic.";
    addOrUpdateFinding(f, streamDesc,
                       formatString("%s: onErrorBeforeClose(%s) fired; rerun with stable routing.",
                                    phaseLabel, convertToText(err)));
    recordWarn();
    return true;
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
    if (!mThreadEnabled.load()) {
        closeStream();
        return;
    }
    if (result != Result::OK || !mStream) {
        setScorecardConfig(rowIndex, formatString("openStream failed (%s)", convertToText(result)));
        setScorecardCell(rowIndex, 1, "SKIP");
        setScorecardCell(rowIndex, 2, "SKIP");
        setScorecardCell(rowIndex, 3, "SKIP");
        appendTelemetryLine(formatString("  openStream failed: %s (%d) [SKIP]",
                                         convertToText(result), static_cast<int>(result)));
        appendTelemetryLine("");
        return;
    }

    const bool isMmap = AAudioExtensions::getInstance().isMMapUsed(mStream.get());
    const SharingMode actualSharing = mStream->getSharingMode();
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t channelCount = mStream->getChannelCount();
    const int32_t burst = mStream->getFramesPerBurst();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t deviceId = mStream->getDeviceId();
    const double capMs = framesToMs(capacity, sampleRate);
    const double burstMs = framesToMs(burst, sampleRate);
    const std::string rateKHzStr = formatSampleRateKHz(sampleRate);

    setScorecardConfig(rowIndex,
                       formatString("%s, %dch, buf=%df/%.0fms, burst=%df",
                                    rateKHzStr.c_str(), channelCount, capacity, capMs, burst));

    appendTelemetryLine(formatString(
            "  Config: MMAP=%s | Mode=%s | %dch | DevId=%d",
            isMmap ? "YES" : "NO",
            (actualSharing == SharingMode::Exclusive) ? "EXCLUSIVE" : "SHARED",
            channelCount,
            deviceId));
    appendTelemetryLine(formatString(
            "  Buffer: %d Hz | %df (%.1fms) | Burst=%df (%.1fms)",
            sampleRate, capacity, capMs, burst, burstMs));
    appendTelemetryLine("");

    if (!isMmap) {
        setScorecardConfig(rowIndex, "Non-MMAP (Fallback)");
        setScorecardCell(rowIndex, 1, "SKIP");
        setScorecardCell(rowIndex, 2, "SKIP");
        setScorecardCell(rowIndex, 3, "SKIP");
        appendTelemetryLine("  [SKIP] Stream did not open as MMAP.");
        appendTelemetryLine("");
        closeStream();
        return;
    }
    if (sharingMode == SharingMode::Exclusive && actualSharing != SharingMode::Exclusive) {
        setScorecardConfig(rowIndex, "Fell back to SHARED");
        setScorecardCell(rowIndex, 1, "SKIP");
        setScorecardCell(rowIndex, 2, "SKIP");
        setScorecardCell(rowIndex, 3, "SKIP");
        appendTelemetryLine("  [SKIP] Requested EXCLUSIVE fell back to SHARED.");
        appendTelemetryLine("");
        closeStream();
        return;
    }

    const std::string streamDesc = formatString("%s %s (%d Hz, %dch, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate, channelCount,
                                                capacity, capMs, burst);

    auto recordReopenFailure = [&](const char *phaseLabel,
                                   bool *inoutFailed,
                                   std::string *inoutTag,
                                   Result openRes) {
        std::string reason;
        if (openRes != Result::OK || !mStream) {
            reason = formatString("openStream returned %s (%d)",
                                  convertToText(openRes), static_cast<int>(openRes));
        } else if (!AAudioExtensions::getInstance().isMMapUsed(mStream.get())) {
            reason = "reopened stream fell back to non-MMAP Legacy path";
        } else if (mStream->getSharingMode() != sharingMode) {
            reason = "reopened stream fell back from EXCLUSIVE to SHARED";
        } else {
            reason = "unknown reopen state mismatch";
        }
        *inoutFailed = true;
        *inoutTag = "FAIL (reopen failed)";
        Finding f;
        f.id = "STREAM-REOPEN-OR-START-FAILURE";
        f.severity = "FAIL";
        f.title = "MMAP stream failed to reopen on MMAP path during diagnostic phase";
        f.whatHappened = "Reopening the MMAP stream for a subsequent test phase failed, fell back to non-MMAP, or lost its requested sharing mode.";
        f.androidContract = "AAudio.h: Opening a valid MMAP stream configuration after closing a previous stream (with stable routing and no competing exclusive owner) should succeed on the MMAP path.";
        f.whyItMatters = "May indicate an unreleased endpoint/PCM handle on close(), or another client/route change preempting the MMAP endpoint.";
        f.howToFix = "Check logcat for AAudioServiceEndpointMMAP and HAL close/open errors to verify PCM handles and shared memory FDs are released promptly on close(), and ensure no other audio app preempted the endpoint.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s (%s): %s.",
                                        dirStr, shareStr, phaseLabel, reason.c_str()));
        recordFail();
    };

    if (flags.runSteadyState && mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 1, "RUNNING...");
        runSteadyStateCheck(direction, rowIndex);
    }

    if (flags.runRapidCycles && mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 2, "RUNNING...");
        bool rapidFailed = false;
        bool rapidDisconnected = false;
        std::string rapidFailureTag = "FAIL";
        if (direction == Direction::Output && mThreadEnabled.load()) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK && mStream &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get()) &&
                mStream->getSharingMode() == sharingMode) {
                runRapidCyclesCheck(direction, true /* usePauseFlush */, rowIndex,
                                    &rapidFailed, &rapidDisconnected, &rapidFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure("Phase 2 Pause/Flush", &rapidFailed, &rapidFailureTag, openRes);
            }
        }
        if (mThreadEnabled.load() && !rapidDisconnected) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK && mStream &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get()) &&
                mStream->getSharingMode() == sharingMode) {
                runRapidCyclesCheck(direction, false /* usePauseFlush (stop->start) */, rowIndex,
                                    &rapidFailed, &rapidDisconnected, &rapidFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure("Phase 2 Stop/Start", &rapidFailed, &rapidFailureTag, openRes);
            }
        }
        if (!mThreadEnabled.load()) {
            setScorecardCell(rowIndex, 2, "STOPPED");
        } else if (rapidDisconnected) {
            setScorecardCell(rowIndex, 2, "DISCONNECTED (route changed — rerun)");
        } else {
            setScorecardCell(rowIndex, 2, rapidFailed ? rapidFailureTag : "PASS");
        }
    }

    if (flags.runStandbyResume && mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 3, "RUNNING...");
        bool standbyFailed = false;
        bool standbyDisconnected = false;
        std::string standbyFailureTag = "FAIL";
        const bool isSharedMode = (sharingMode == SharingMode::Shared);
        if (direction == Direction::Output && mThreadEnabled.load()) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK && mStream &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get()) &&
                mStream->getSharingMode() == sharingMode) {
                runStandbyResumeCheck(direction, true /* usePauseFlush */, rowIndex,
                                      &standbyFailed, &standbyDisconnected, &standbyFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure(isSharedMode ? "Phase 3 Pause/Idle" : "Phase 3 Pause/Standby",
                                    &standbyFailed, &standbyFailureTag, openRes);
            }
        }
        if (mThreadEnabled.load() && !standbyDisconnected) {
            Result openRes = openMmapStream(direction, sharingMode);
            if (openRes == Result::OK && mStream &&
                AAudioExtensions::getInstance().isMMapUsed(mStream.get()) &&
                mStream->getSharingMode() == sharingMode) {
                runStandbyResumeCheck(direction, false /* usePauseFlush (stop->standby->start) */, rowIndex,
                                      &standbyFailed, &standbyDisconnected, &standbyFailureTag);
            } else if (mThreadEnabled.load()) {
                recordReopenFailure(isSharedMode ? "Phase 3 Stop/Idle" : "Phase 3 Stop/Standby",
                                    &standbyFailed, &standbyFailureTag, openRes);
            }
        }
        if (!mThreadEnabled.load()) {
            setScorecardCell(rowIndex, 3, "STOPPED");
        } else if (standbyDisconnected) {
            setScorecardCell(rowIndex, 3, "DISCONNECTED (route changed — rerun)");
        } else {
            setScorecardCell(rowIndex, 3, standbyFailed ? standbyFailureTag : "PASS");
        }
    }

    closeStream();
    appendTelemetryLine("");
}

void TestMmapTimestamp::runSteadyStateCheck(Direction direction, size_t rowIndex) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const bool isShared = (mStream->getSharingMode() == SharingMode::Shared);
    const char *shareStr = isShared ? "SHARED" : "EXCLUSIVE";
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t channelCount = mStream->getChannelCount();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t burst = mStream->getFramesPerBurst();
    const double burstMs = framesToMs(burst, sampleRate);
    const std::string streamDesc = formatString("%s %s (%d Hz, %dch, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate, channelCount,
                                                capacity, framesToMs(capacity, sampleRate), burst);

    appendTelemetryLine("  [Phase 1] Steady-State & Cold-Start DMA (1.5s post-start)");
    mDataCallback->reset();
    if (mErrorCallback) mErrorCallback->reset();

    const int64_t startCallNs = AudioClock::getNanoseconds();
    Result res = mStream->start(kStateChangeTimeoutNs);
    const int64_t postStartNs = AudioClock::getNanoseconds();
    const double startDurationMs =
            static_cast<double>(postStartNs - startCallNs) / NANOS_PER_MILLISECOND;

    if (!mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 1, "STOPPED");
        return;
    }
    if (checkAndReportDisconnect(streamDesc, "Phase 1 start", rowIndex, 1)) {
        return;
    }
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: start() returned %s (%.1fms)",
                                         convertToText(res), startDurationMs));
        appendTelemetryLine("");
        setScorecardCell(rowIndex, 1, "FAIL (start failed)");
        Finding f;
        f.id = "STEADY-START-OR-TIMESTAMP-UNAVAILABLE";
        f.severity = "FAIL";
        f.title = "MMAP stream failed to start or produce valid timestamps during steady-state run";
        f.whatHappened = "Starting the MMAP stream or polling getTimestamp(CLOCK_MONOTONIC) during 1.5s of active streaming failed.";
        f.androidContract = "AAudio.h: An open MMAP stream must start cleanly and provide advancing (framePosition, timeNanoseconds) pairs via AAudioStream_getTimestamp().";
        f.whyItMatters = "Applications cannot play/capture audio or synchronize audio/video clocks without a working MMAP stream and valid timestamps.";
        f.howToFix = "Check whether AAudioServiceStreamMMAP / HAL IStreamCommon::sendCommand(start/burst) failed to start the PCM stream or failed to report valid timestamps.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: start() returned %s after %.1f ms.",
                                        dirStr, shareStr, convertToText(res), startDurationMs));
        recordFail();
        return;
    }

    int64_t firstHwAdvanceNs = 0;
    std::vector<TimestampSample> samples;
    samples.reserve(450);

    int32_t validTimestampPolls = 0;
    int32_t distinctPosChanges = 0;
    int32_t staleFrameNewTimeCount = 0;
    int32_t staleSpanOverBurstCount = 0;
    double maxStaleFrameDeltaMs = 0.0;
    int64_t staleFrameExamplePos = 0;
    int32_t backwardPosCount = 0;
    int32_t backwardTimeCount = 0;
    int32_t rateOutlierCount = 0;
    double minInstRate = 1e12;
    double maxInstRate = 0.0;

    const int64_t staleBurstThresholdNs = std::max<int64_t>(
            (sampleRate > 0) ? ((static_cast<int64_t>(burst) * NANOS_PER_SECOND) / sampleRate) : 0,
            5LL * NANOS_PER_MILLISECOND);

    TimestampSample prevPollSample{};
    TimestampSample lastDistinctPosSample{};
    bool hasPrevPoll = false;

    const int64_t testEndNs = postStartNs + 1500LL * NANOS_PER_MILLISECOND;
    while (AudioClock::getNanoseconds() < testEndNs && mThreadEnabled.load()) {
        if (mErrorCallback && mErrorCallback->isDisconnected()) {
            break;
        }
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

            if (hasPrevPoll) {
                if (s.tsPosition < prevPollSample.tsPosition) backwardPosCount++;
                if (s.tsTimeNs < prevPollSample.tsTimeNs) backwardTimeCount++;

                const int64_t dPosDistinct = s.tsPosition - lastDistinctPosSample.tsPosition;
                const int64_t dTimeDistinctNs = s.tsTimeNs - lastDistinctPosSample.tsTimeNs;
                const bool warmUpDone = (distinctPosChanges >= 2) &&
                        ((nowNs - postStartNs) >= 200LL * NANOS_PER_MILLISECOND);

                if (dPosDistinct == 0 && s.tsTimeNs > prevPollSample.tsTimeNs) {
                    if (warmUpDone) {
                        staleFrameNewTimeCount++;
                        if (dTimeDistinctNs > staleBurstThresholdNs) {
                            staleSpanOverBurstCount++;
                        }
                        const double deltaMs =
                                static_cast<double>(dTimeDistinctNs) / NANOS_PER_MILLISECOND;
                        if (deltaMs > maxStaleFrameDeltaMs) {
                            maxStaleFrameDeltaMs = deltaMs;
                            staleFrameExamplePos = s.tsPosition;
                        }
                    }
                } else if (dPosDistinct > 0 && dTimeDistinctNs > 0) {
                    distinctPosChanges++;
                    const double instRate =
                            (static_cast<double>(dPosDistinct) * NANOS_PER_SECOND) /
                            static_cast<double>(dTimeDistinctNs);
                    minInstRate = std::min(minInstRate, instRate);
                    maxInstRate = std::max(maxInstRate, instRate);
                    if (instRate < 0.5 * sampleRate || instRate > 1.5 * sampleRate) {
                        rateOutlierCount++;
                    }
                    samples.push_back(s);
                }
                if (s.tsPosition != lastDistinctPosSample.tsPosition) {
                    lastDistinctPosSample = s;
                }
            } else {
                samples.push_back(s);
                lastDistinctPosSample = s;
                hasPrevPoll = true;
            }
            prevPollSample = s;
        }

        const int64_t elapsedMs = (nowNs - postStartNs) / NANOS_PER_MILLISECOND;
        const int32_t sleepMs = (elapsedMs < 100) ? 2 : 4;
        AudioClock::sleepForNanos(static_cast<int64_t>(sleepMs) * NANOS_PER_MILLISECOND);
    }

    if (!mThreadEnabled.load()) {
        setScorecardCell(rowIndex, 1, "STOPPED");
        return;
    }
    if (checkAndReportDisconnect(streamDesc, "Phase 1 steady-state", rowIndex, 1)) {
        return;
    }

    const int64_t endPollNs = AudioClock::getNanoseconds();
    const int64_t firstCbNs = mDataCallback->getFirstCallbackTimeNs();
    const double firstCbMs = (firstCbNs > 0)
            ? static_cast<double>(firstCbNs - startCallNs) / NANOS_PER_MILLISECOND
            : -1.0;
    const double firstHwMs = (firstHwAdvanceNs > 0)
            ? static_cast<double>(firstHwAdvanceNs - startCallNs) / NANOS_PER_MILLISECOND
            : -1.0;
    const double hwLagAfterCbMs = (firstCbNs > 0)
            ? ((firstHwAdvanceNs > firstCbNs)
                    ? static_cast<double>(firstHwAdvanceNs - firstCbNs) / NANOS_PER_MILLISECOND
                    : ((firstHwAdvanceNs == 0)
                            ? static_cast<double>(endPollNs - firstCbNs) / NANOS_PER_MILLISECOND
                            : 0.0))
            : 0.0;

    appendTelemetryLine(formatString(
            "    Cold-Start : start()=%.1fms, 1stCb=%.1fms, 1stDma=%.1fms (lag=%.1fms)",
            startDurationMs, firstCbMs, firstHwMs, hwLagAfterCbMs));
    appendTelemetryLine(formatString(
            "    Timestamps : %d valid, %zu distinct, %d stale>warmup (%d >1burst, max=%.1fms)",
            validTimestampPolls, samples.size(), staleFrameNewTimeCount,
            staleSpanOverBurstCount, maxStaleFrameDeltaMs));

    if (samples.size() < 5) {
        appendTelemetryLine("    Result     : FAIL (<5 distinct timestamps)");
        appendTelemetryLine("");
        setScorecardCell(rowIndex, 1, "FAIL (no timestamps)");
        Finding f;
        f.id = "STEADY-START-OR-TIMESTAMP-UNAVAILABLE";
        f.severity = "FAIL";
        f.title = "MMAP stream failed to start or produce valid timestamps during steady-state run";
        f.whatHappened = "During 1.5s of post-start active streaming, getTimestamp(CLOCK_MONOTONIC) returned fewer than 5 distinct timestamp updates.";
        f.androidContract = "AAudio.h: An active MMAP stream must report advancing (framePosition, timeNanoseconds) presentation timestamps.";
        f.whyItMatters = "Without advancing timestamps, apps cannot compute audio latency, A/V lip-sync, or clock drift.";
        f.howToFix = "Check whether HAL Reply.observable is stuck/unpopulated or AAudioServiceEndpointMMAP is failing timestamp updates while streaming.";
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
        f.androidContract = "AAudioStream_getTimestamp() and StreamDescriptor.aidl (Reply.observable): Both frame position and timeNs must be non-decreasing while a stream is actively running.";
        f.whyItMatters = "Causes audio/video sync jumps, glitches, and erratic clock drift recovery in media playback and VoIP apps.";
        f.howToFix = "Check whether the HAL Reply.observable (frames, timeNs) latch has a race/wraparound or whether AAudioServiceEndpointMMAP switched timestamp sources mid-stream.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: backwardPos=%d, backwardTime=%d during 1.5s steady run.",
                                        dirStr, shareStr, backwardPosCount, backwardTimeCount));
    }

    // Only warn on stale-frame-with-updated-clock after warm-up when stale spans exceed >1 burst
    // repeatedly AND measurably elevate steady-state timestamp jitter.
    if (staleSpanOverBurstCount >= 3 &&
        maxStaleFrameDeltaMs > std::max(burstMs * 1.5, 6.0) &&
        rmsResidualMs > std::max(burstMs * 0.35, 1.0)) {
        if (!checkFailed) {
            checkWarned = true;
            statusTag = formatString("WARN (stale ts %.1fms)", maxStaleFrameDeltaMs);
        }
        Finding f;
        f.id = "STEADY-STALE-FRAME-UPDATED-CLOCK";
        f.severity = "WARNING";
        f.title = "Timestamp clock (timeNs) advances across >1 burst while frame position is unchanged";
        f.whatHappened = "After cold-start warm-up, multiple getTimestamp() polls observed an unchanged frame position across more than a full burst while timeNs continued advancing with the host clock, increasing sawtooth timestamp jitter.";
        f.androidContract = "AAudio.h (AAudioStream_getTimestamp): Best accuracy is achieved when (framePosition, timeNanoseconds) is latched when the hardware DMA/DSP counter updates rather than overwriting timeNs with poll time across multi-burst stalls (note: AAudioServiceEndpointMMAP tolerates brief frozen windows during its grace period, and default reference HALs may stamp query time).";
        f.whyItMatters = "Advancing timeNs while frames stays constant across multiple bursts introduces sawtooth jitter proportional to the stall interval, degrading fine A/V sync and drift estimation.";
        f.howToFix = "Possible causes: (1) HAL populateReply stamping current uptimeNanos() when hardware position only updates coarsely every N bursts — prefer latching (frames, timeNs) at DMA/DSP burst interrupts; or (2) Coarse DSP position reporting period.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: Post-warmup stale-frame polls=%d (%d spanning >1 burst, max span=%.2f ms at %lldf), RMS jitter=%.2f ms.",
                                        dirStr, shareStr, staleFrameNewTimeCount, staleSpanOverBurstCount,
                                        maxStaleFrameDeltaMs, static_cast<long long>(staleFrameExamplePos),
                                        rmsResidualMs));
    }

    if (std::abs(rateErrorPpm) > 5000.0 || maxAbsResidualMs > 15.0) {
        const bool isFail = (std::abs(rateErrorPpm) > 20000.0 || maxAbsResidualMs > 50.0);
        const std::string detailStr = (std::abs(rateErrorPpm) > 5000.0)
                ? formatString("rate %+.0fppm", rateErrorPpm)
                : formatString("jitter %.1fms", maxAbsResidualMs);
        if (isFail) {
            checkFailed = true;
            statusTag = formatString("FAIL (%s)", detailStr.c_str());
        } else if (!checkFailed) {
            checkWarned = true;
            statusTag = formatString("WARN (%s)", detailStr.c_str());
        }
        Finding f;
        f.id = "STEADY-CLOCK-DRIFT-OR-JITTER";
        f.severity = isFail ? "FAIL" : "WARNING";
        f.title = "Excessive steady-state timestamp rate error or linear-fit jitter";
        f.whatHappened = "During continuous 1.5s post-start streaming, the linear regression slope of getTimestamp() deviated significantly from the nominal sample rate or exhibited high residual jitter.";
        f.androidContract = "AAudio.h (AAudioStream_getTimestamp): Reported (framePosition, timeNanoseconds) pairs should track the hardware sample clock smoothly with minimal jitter.";
        f.whyItMatters = "High timestamp jitter or clock rate error degrades A/V lip-sync, adaptive resampling, and acoustic echo cancellation.";
        f.howToFix = "Capture the hardware DMA counter and CLOCK_MONOTONIC timestamp together in the kernel/DSP interrupt or ALSA status query rather than delayed userspace polling, and check for AAudioServiceEndpointMMAP clock-model resync jumps.";
        addOrUpdateFinding(f, streamDesc,
                           formatString("%s %s: Linear fit rate=%.1f Hz (%+.0f ppm vs %d Hz), RMS jitter=%.2f ms, Max residual=%.2f ms (outliers=%d).",
                                        dirStr, shareStr, slopeHz, rateErrorPpm, sampleRate,
                                        rmsResidualMs, maxAbsResidualMs, rateOutlierCount));
    }

    if (direction == Direction::Output) {
        const bool dmaBehindTs = (avgPipelineOffsetMs < -1.0);
        const bool zeroSeparationWithStartupLag =
                (!dmaBehindTs && hwLagAfterCbMs >= 12.0 && avgPipelineOffsetMs < 0.2);
        if (dmaBehindTs || zeroSeparationWithStartupLag) {
            const bool isFail = dmaBehindTs;
            if (isFail) {
                checkFailed = true;
                statusTag = "FAIL (dma < ts)";
            } else if (!checkFailed) {
                checkWarned = true;
                statusTag = formatString("WARN (dma ~= ts, %.0fms lag)", hwLagAfterCbMs);
            }
            Finding f;
            f.id = "STEADY-OUTPUT-DMA-SUBTRACTS-LATENCY";
            f.severity = isFail ? "FAIL" : "WARNING";
            if (dmaBehindTs) {
                f.title = "Output read counter (getFramesRead) lags behind speaker presentation timestamp (getTimestamp)";
                f.whatHappened = "In steady state on an output MMAP stream, projected getFramesRead() lagged behind getTimestamp().position (dma - ts < -1.0 ms). Physically, frames must be read from the MMAP ring buffer before or when they reach the speaker.";
            } else {
                f.title = "Output read counter (getFramesRead) shows startup lag and zero speaker pipeline separation from getTimestamp()";
                f.whatHappened = "On an output MMAP stream, getFramesRead() stayed at 0 for >= 12 ms after the first callback and steady-state (getFramesRead() - getTimestamp().position) remained ~0 ms instead of reflecting positive downstream DSP/DAC latency.";
            }
            f.androidContract = "StreamDescriptor.aidl (Reply): Reply.hardware reports the unadjusted ring-buffer DMA read position, while Reply.observable reports speaker presentation position (downstream of DMA). Note that client getFramesRead() is estimated via AAudio's IsochronousClockModel from service timestamps.";
            f.whyItMatters = "When Reply.hardware subtracts downstream speaker latency (or Reply.observable omits speaker latency / falls back to synthetic DMA offsets), client buffer-occupancy estimates (framesWritten - framesRead) and end-to-end latency calculations are skewed.";
            f.howToFix = "Possible causes to inspect:\n  1. HAL Reply.hardware vs Reply.observable: Ensure Reply.hardware reports the raw ring-buffer DMA counter without subtracting DSP/speaker latency, and account for downstream speaker latency only in Reply.observable.\n  2. AAudioServiceEndpointMMAP fallback / SHARED mixer: If HAL getExternalPosition ever returned an error once, AAudioServiceEndpointMMAP permanently latches fallback to a synthetic fixed offset from the clock model; on SHARED streams, timestamps pass through the AAudio service mixer.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s: 1stDma lag after 1stCb=%.1f ms, steady-state (projected getFramesRead() - getTimestamp().position) = %+.2f ms (%+.1f frames).",
                                            dirStr, shareStr, hwLagAfterCbMs, avgPipelineOffsetMs, avgPipelineOffsetFrames));
        }
    }

    if (checkFailed) {
        appendTelemetryLine("    Result     : FAIL");
        appendTelemetryLine("");
        setScorecardCell(rowIndex, 1, statusTag);
        recordFail();
    } else if (checkWarned) {
        appendTelemetryLine("    Result     : WARN");
        appendTelemetryLine("");
        setScorecardCell(rowIndex, 1, statusTag);
        recordWarn();
    } else {
        appendTelemetryLine("    Result     : PASS");
        appendTelemetryLine("");
        setScorecardCell(rowIndex, 1, "PASS");
        recordPass();
    }
}

int64_t TestMmapTimestamp::computeJumpThresholdFrames(int32_t sampleRate,
                                                      int32_t framesPerBurst,
                                                      bool isShared) {
    const int64_t burst = std::max<int64_t>(1, framesPerBurst);
    const int64_t sr = std::max<int64_t>(1, sampleRate);
    // Capacity-independent burst/time tolerance:
    // - EXCLUSIVE: max(4 bursts, 10 ms)
    // - SHARED   : max(6 bursts, 15 ms) to allow for AAudio service mixer scheduling margin
    if (isShared) {
        return std::max<int64_t>(burst * 6, (sr * 15) / 1000);
    }
    return std::max<int64_t>(burst * 4, (sr * 10) / 1000);
}

bool TestMmapTimestamp::runUntilUnalignedPosition(int64_t cycleBaseHw,
                                                  int32_t minRunMs,
                                                  int32_t bufferCapacity,
                                                  int32_t framesPerBurst,
                                                  int64_t jumpThreshold) {
    if (!sleepMillisInterruptible(minRunMs)) {
        return false;
    }
    if (bufferCapacity <= 0) {
        return true;
    }
    const int32_t desiredMargin = static_cast<int32_t>(
            std::min<int64_t>(bufferCapacity * 2 / 5,
                              jumpThreshold + std::max(framesPerBurst * 2, 96)));
    const int32_t minRem = std::max(std::min(bufferCapacity / 4, std::max(framesPerBurst, 48)),
                                    desiredMargin);
    const int32_t maxRem = std::max(minRem + std::max(framesPerBurst, 48),
                                    bufferCapacity - minRem);
    for (int32_t attempt = 0; attempt < 60 && mThreadEnabled.load(); ++attempt) {
        if (mErrorCallback && mErrorCallback->isDisconnected()) {
            return false;
        }
        const int64_t hw = (mStream->getDirection() == Direction::Output)
                ? mStream->getFramesRead()
                : mStream->getFramesWritten();
        const int64_t rem = (bufferCapacity > 0)
                ? (std::max<int64_t>(0, hw - cycleBaseHw) % bufferCapacity)
                : 0;
        int64_t tsPos = 0;
        int64_t tsTime = 0;
        const Result tsRes = mStream->getTimestamp(CLOCK_MONOTONIC, &tsPos, &tsTime);
        const int64_t nowNs = AudioClock::getNanoseconds();
        const bool tsFresh = (tsRes == Result::OK) &&
                (nowNs - tsTime >= -20LL * NANOS_PER_MILLISECOND) &&
                (nowNs - tsTime <= 60LL * NANOS_PER_MILLISECOND);
        if (rem >= minRem && rem <= maxRem && (tsFresh || attempt >= 40)) {
            return true;
        }
        AudioClock::sleepForNanos(3 * NANOS_PER_MILLISECOND);
    }
    return mThreadEnabled.load() && (!mErrorCallback || !mErrorCallback->isDisconnected());
}

bool TestMmapTimestamp::waitForFreshTimestamp(int64_t prevTsTimeNs,
                                              int32_t timeoutMs,
                                              TimestampSample *outSample,
                                              Result *outLastResult) {
    Result lastRes = Result::ErrorTimeout;
    const int64_t deadlineNs =
            AudioClock::getNanoseconds() + static_cast<int64_t>(timeoutMs) * NANOS_PER_MILLISECOND;
    while (AudioClock::getNanoseconds() < deadlineNs && mThreadEnabled.load()) {
        if (mErrorCallback && mErrorCallback->isDisconnected()) {
            if (outLastResult) *outLastResult = mErrorCallback->getError();
            return false;
        }
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
    if (lastRes == Result::OK) {
        lastRes = Result::ErrorTimeout;
    }
    if (outLastResult) *outLastResult = lastRes;
    return false;
}

bool TestMmapTimestamp::projectTimestampToHostTime(const TimestampSample &s,
                                                   int32_t sampleRate,
                                                   int64_t *outProjectedFrames,
                                                   double *outAgeMs) {
    const int64_t dtNs = s.hostTimeNs - s.tsTimeNs;
    const double ageMs = static_cast<double>(dtNs) / NANOS_PER_MILLISECOND;
    if (outAgeMs != nullptr) {
        *outAgeMs = ageMs;
    }
    if (sampleRate <= 0 || s.tsTimeNs <= 0 || s.hostTimeNs <= 0) {
        *outProjectedFrames = s.tsPosition;
        return false;
    }
    // Signed age projection across [-50 ms, +250 ms] so slightly future-dated HAL presentation
    // timestamps and normal aged timestamps are projected consistently without silent fallback.
    if (dtNs < -50LL * NANOS_PER_MILLISECOND || dtNs > 250LL * NANOS_PER_MILLISECOND) {
        const int64_t signedOffset = std::llround(
                (static_cast<double>(dtNs) * static_cast<double>(sampleRate)) / NANOS_PER_SECOND);
        *outProjectedFrames = s.tsPosition + signedOffset;
        return false;
    }
    const int64_t signedAgeFrames = std::llround(
            (static_cast<double>(dtNs) * static_cast<double>(sampleRate)) / NANOS_PER_SECOND);
    *outProjectedFrames = s.tsPosition + signedAgeFrames;
    return true;
}

void TestMmapTimestamp::runRapidCyclesCheck(Direction direction,
                                            bool usePauseFlush,
                                            size_t rowIndex,
                                            bool *inoutAnyFailed,
                                            bool *inoutDisconnected,
                                            std::string *inoutFailureTag) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const bool isShared = (mStream->getSharingMode() == SharingMode::Shared);
    const char *shareStr = isShared ? "SHARED" : "EXCLUSIVE";
    const char *modeStr = usePauseFlush ? "Pause -> Flush -> Start" : "Stop -> Start";
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t channelCount = mStream->getChannelCount();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t burst = mStream->getFramesPerBurst();
    const double capMs = framesToMs(capacity, sampleRate);
    const int64_t jumpThreshold = computeJumpThresholdFrames(sampleRate, burst, isShared);
    const std::string streamDesc = formatString("%s %s (%d Hz, %dch, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate, channelCount,
                                                capacity, capMs, burst);

    appendTelemetryLine(formatString("  [Phase 2] Rapid %s (%d cycles, tol=+/-%lldf/%.1fms)",
                                     modeStr, kRapidCycleCount,
                                     static_cast<long long>(jumpThreshold),
                                     framesToMs(jumpThreshold, sampleRate)));

    auto recordCycleControlFinding = [&](const std::string &tag, const std::string &detail) {
        *inoutAnyFailed = true;
        *inoutFailureTag = tag;
        Finding f;
        f.id = "CYCLE-STREAM-CONTROL-OR-TIMESTAMP-FAILURE";
        f.severity = "FAIL";
        f.title = "Stream state transition or getTimestamp() failed during rapid Stop/Start cycles";
        f.whatHappened = "During rapid start/pause/flush/stop cycles, starting or stopping the stream failed, getTimestamp(CLOCK_MONOTONIC) failed to advance, or timestamp age fell outside [-50ms, +250ms].";
        f.androidContract = "AAudio.h: Rapid start(), pause(), flush(), and stop() transitions must succeed and resume producing fresh advancing timestamps.";
        f.whyItMatters = "Causes playback or capture to stall or lose timestamp synchronization after quick pause/resume or seek operations.";
        f.howToFix = "Check both AAudioServiceStreamMMAP state transitions and HAL start/pause/flush/stop handling to ensure PCM DMA and timestamp reporting resume cleanly.";
        addOrUpdateFinding(f, streamDesc, detail);
    };

    mDataCallback->reset();
    if (mErrorCallback) mErrorCallback->reset();
    Result res = mStream->start(kStateChangeTimeoutNs);
    if (!mThreadEnabled.load()) return;
    if (checkAndReportDisconnect(streamDesc, "Phase 2 initial start", rowIndex, 0)) {
        *inoutDisconnected = true;
        return;
    }
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: initial start() returned %s", convertToText(res)));
        appendTelemetryLine("");
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
    int32_t unalignedRemJumpCount = 0;
    int64_t exampleTsError = 0;
    int64_t exampleHwError = 0;
    int64_t exampleRingOffset = 0;
    int64_t cycleBaseHw = 0;

    for (int32_t cycle = 1; cycle <= kRapidCycleCount && mThreadEnabled.load(); ++cycle) {
        const int64_t cycleRunStartNs = AudioClock::getNanoseconds();
        const int32_t minRunMs = (cycle == 1) ? 120 : 45;
        if (!runUntilUnalignedPosition(cycleBaseHw, minRunMs, capacity, burst, jumpThreshold)) {
            if (checkAndReportDisconnect(streamDesc, "Phase 2 cycle run", rowIndex, 0)) {
                *inoutDisconnected = true;
                return;
            }
            break;
        }

        TimestampSample preSample{};
        Result lastTsRes = Result::OK;
        if (!waitForFreshTimestamp(cycleRunStartNs, 500, &preSample, &lastTsRes)) {
            if (!mThreadEnabled.load()) break;
            if (checkAndReportDisconnect(streamDesc, "Phase 2 pre-stop timestamp", rowIndex, 0)) {
                *inoutDisconnected = true;
                return;
            }
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
        const int64_t preRem = (capacity > 0)
                ? (std::max<int64_t>(0, preHw - cycleBaseHw) % capacity)
                : 0;
        const int64_t preComp = (capacity > 0) ? (capacity - preRem) : 0;

        if (usePauseFlush) {
            res = mStream->pause(kStateChangeTimeoutNs);
            if (res == Result::OK) res = mStream->flush(kStateChangeTimeoutNs);
        } else {
            res = mStream->stop(kStateChangeTimeoutNs);
        }
        if (!mThreadEnabled.load()) break;
        if (checkAndReportDisconnect(streamDesc, "Phase 2 stop/flush", rowIndex, 0)) {
            *inoutDisconnected = true;
            return;
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
        res = mStream->start(kStateChangeTimeoutNs);
        if (!mThreadEnabled.load()) break;
        if (checkAndReportDisconnect(streamDesc, "Phase 2 restart", rowIndex, 0)) {
            *inoutDisconnected = true;
            return;
        }
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
            if (checkAndReportDisconnect(streamDesc, "Phase 2 post-start run", rowIndex, 0)) {
                *inoutDisconnected = true;
                return;
            }
            break;
        }

        TimestampSample postSample{};
        if (!waitForFreshTimestamp(restartCallNs, 800, &postSample, &lastTsRes)) {
            if (!mThreadEnabled.load()) break;
            if (checkAndReportDisconnect(streamDesc, "Phase 2 post-start timestamp", rowIndex, 0)) {
                *inoutDisconnected = true;
                return;
            }
            appendTelemetryLine(formatString("    C%d [FAIL]: no fresh timestamp post-start (%s)",
                                             cycle, convertToText(lastTsRes)));
            subtestFailed = true;
            recordCycleControlFinding(
                    "FAIL (no post-start ts)",
                    formatString("%s %s (%s) C%d: no fresh timestamp within 800 ms after restart (%s).",
                                 dirStr, shareStr, modeStr, cycle, convertToText(lastTsRes)));
            break;
        }

        int64_t preProjTs = 0;
        int64_t postProjTs = 0;
        double preAgeMs = 0.0;
        double postAgeMs = 0.0;
        const bool preAgeOk = projectTimestampToHostTime(preSample, sampleRate, &preProjTs, &preAgeMs);
        const bool postAgeOk = projectTimestampToHostTime(postSample, sampleRate, &postProjTs, &postAgeMs);
        if (!preAgeOk || !postAgeOk) {
            appendTelemetryLine(formatString(
                    "    C%d [FAIL]: excessive timestamp age (pre=%.1fms, post=%.1fms)",
                    cycle, preAgeMs, postAgeMs));
            subtestFailed = true;
            recordCycleControlFinding(
                    "FAIL (stale ts age)",
                    formatString("%s %s (%s) C%d: getTimestamp() age out of [-50ms, +250ms] bounds (pre=%.1f ms, post=%.1f ms).",
                                 dirStr, shareStr, modeStr, cycle, preAgeMs, postAgeMs));
            break;
        }

        const int64_t dWritten = postSample.framesWritten - preSample.framesWritten;
        const int64_t dRead = postSample.framesRead - preSample.framesRead;
        const int64_t dTsPos = postProjTs - preProjTs;
        const int64_t dApp = (direction == Direction::Output) ? dWritten : dRead;
        const int64_t dHw = (direction == Direction::Output) ? dRead : dWritten;

        const int64_t tsError = dTsPos - dApp;
        const int64_t dmaError = dHw - dApp;
        cumulativeTsDrift += tsError;
        cumulativeHwDrift += dmaError;
        cycleBaseHw = preHw + dmaError;

        const bool backwardTs = (postSample.tsPosition < preSample.tsPosition);
        const bool causalViolationOut = (direction == Direction::Output) &&
                ((postSample.tsPosition > postSample.framesWritten + burst) ||
                 (postSample.framesRead > postSample.framesWritten + burst));
        const bool largeJump = (std::abs(tsError) > jumpThreshold) ||
                (std::abs(dmaError) > jumpThreshold);

        const bool cycleFail = backwardTs || causalViolationOut || largeJump;
        appendTelemetryLine(formatString(
                "    C%d [%s] @%-4lldf: ts=%+lldf(%+.1fms) dma=%+lldf",
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

            const int64_t classifyTol = std::max<int64_t>(burst * 3, (sampleRate * 6LL) / 1000);
            if (capacity > 0 && tsError > 0 && std::abs(tsError - capacity) <= classifyTol * 2) {
                fullCapForwardJumpCount++;
            } else if (!backwardTs && preRem > 0 &&
                       (std::abs(tsError - preRem) <= classifyTol ||
                        std::abs(dmaError - preRem) <= classifyTol ||
                        std::abs(tsError + preRem) <= classifyTol ||
                        std::abs(dmaError + preRem) <= classifyTol ||
                        (preComp > 0 && (std::abs(tsError - preComp) <= classifyTol ||
                                         std::abs(dmaError - preComp) <= classifyTol ||
                                         std::abs(tsError + preComp) <= classifyTol ||
                                         std::abs(dmaError + preComp) <= classifyTol)))) {
                unalignedRemJumpCount++;
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
        appendTelemetryLine("    Result    : FAIL");
        appendTelemetryLine("");

        if (hasCycleJumpFailure) {
            if (fullCapForwardJumpCount > 0 && direction == Direction::Input) {
                *inoutFailureTag = formatString("FAIL (%+.0fms jump/stop)",
                                                framesToMs(exampleTsError, sampleRate));
                Finding f;
                f.id = "CYCLE-INPUT-BUFFER-CAPACITY-JUMP";
                f.severity = "FAIL";
                f.title = "Input timestamp jumps forward by +1 buffer capacity on Stop -> Start";
                f.whatHappened = "When the input MMAP stream was stopped at an unaligned ring-buffer offset and restarted, getTimestamp().position / getFramesWritten() jumped forward by ~1 full buffer capacity relative to the frames read by the app.";
                f.androidContract = "AAudio.h & StreamDescriptor.aidl: Across stop() -> start(), capture frame and presentation counters must remain continuous with actual frames transferred without adding full ring-buffer multiples.";
                f.whyItMatters = "Every stop/start shifts capture timestamps forward by a full buffer capacity, breaking Acoustic Echo Cancellation (AEC) and multi-track recording alignment.";
                f.howToFix = "Possible causes:\n  1. AAudioServiceEndpointMMAP::stopStream() calls mFramesTransferred.roundUp64(capacity) assuming the HAL resets its hardware ring pointer to index 0 on restart — if the Input HAL fails to stop/reset the capture PCM driver on pause/stop, the hardware pointer resumes from its old ring offset and adds +capacity per cycle.\n  2. Client/server offset rebasing mismatch in AudioStreamInternalCapture across stop/start.";
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Jumped forward by %+lld frames (%+.1f ms = %+.2fx bufferCapacity) per stop/start (%d/%d cycles); net drift = %+lld frames (%+.1f ms).",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(exampleTsError),
                                                framesToMs(exampleTsError, sampleRate),
                                                capacity > 0 ? (static_cast<double>(exampleTsError) / capacity) : 0.0,
                                                fullCapForwardJumpCount,
                                                kRapidCycleCount,
                                                static_cast<long long>(cumulativeTsDrift),
                                                framesToMs(cumulativeTsDrift, sampleRate)));
            } else if (unalignedRemJumpCount > 0) {
                *inoutFailureTag = "FAIL (ring offset jump)";
                Finding f;
                f.id = "CYCLE-UNALIGNED-RING-OFFSET-JUMP";
                f.severity = "FAIL";
                f.title = "MMAP frame counter jumps by unaligned ring-buffer offset across Stop/Flush";
                f.whatHappened = "Stopping or flushing the MMAP stream at an unaligned ring-buffer offset (rem = (dma - base) % bufferCapacity) and restarting caused client/timestamp counters to shift by the ring offset or its complement (capacity - rem).";
                f.androidContract = "AAudio.h: Client frame counters and getTimestamp() must advance continuously across pause->flush->start and stop->start without leaking ring-buffer alignment offsets.";
                f.whyItMatters = "Causes cumulative timestamp drift and false XRun buffer expansion on every stream flush or restart.";
                f.howToFix = "Possible causes:\n  1. AAudio client/service offset accounting (AudioStreamInternal advanceClientToMatchServerPosition / onFlushFromServer folding vs AAudioServiceEndpointMMAP roundUp64(capacity)).\n  2. HAL cumulative frame counter not aligning with hardware ring-buffer index 0 reset across stop/flush.";
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Jumped by tsError=%+lldf (%+.1f ms), dmaError=%+lldf (%+.1f ms) at ringOffset=%lld/%df.",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(exampleTsError),
                                                framesToMs(exampleTsError, sampleRate),
                                                static_cast<long long>(exampleHwError),
                                                framesToMs(exampleHwError, sampleRate),
                                                static_cast<long long>(exampleRingOffset),
                                                capacity));
            } else {
                *inoutFailureTag = formatString("FAIL (ts jump %+.0fms)",
                                                framesToMs(exampleTsError, sampleRate));
                Finding f;
                f.id = "CYCLE-POSITION-RESET-OR-JUMP";
                f.severity = "FAIL";
                f.title = "MMAP timestamp or frame counter resets or jumps across rapid Start / Stop / Flush cycles";
                f.whatHappened = "Across rapid pause -> flush -> start or stop -> start transitions, projected getTimestamp() or opposite-end frame counters shifted beyond the burst/time continuity bound or regressed.";
                f.androidContract = "AAudio.h & StreamDescriptor.aidl: Across start->stop->start and pause->flush->start transitions (accounting for AAudio's intentional folding of flushed unplayed frames into client offsets), timestamps must remain monotonic and continuous.";
                f.whyItMatters = "Causes A/V lip-sync jumps and corrupts buffer occupancy tracking after pause, seek, or stop.";
                f.howToFix = "Possible causes:\n  1. HAL zeroing its cumulative MMAP frame counter on stop/pause/flush or clearing its pre-stop position latch prematurely before start().\n  2. AAudio client/service mFramesOffsetFromService resync bug across rapid stop/flush transitions (or SHARED mixer counter reset).";
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
        appendTelemetryLine("    Result    : PASS");
        appendTelemetryLine("");
        recordPass();
    }
}

void TestMmapTimestamp::runStandbyResumeCheck(Direction direction,
                                              bool usePauseFlush,
                                              size_t rowIndex,
                                              bool *inoutAnyFailed,
                                              bool *inoutDisconnected,
                                              std::string *inoutFailureTag) {
    const char *dirStr = (direction == Direction::Output) ? "OUTPUT" : "INPUT";
    const bool isExclusive = (mStream->getSharingMode() == SharingMode::Exclusive);
    const char *shareStr = isExclusive ? "EXCLUSIVE" : "SHARED";
    const char *modeStr = isExclusive
            ? (usePauseFlush
                    ? "Pause -> 3.5s Standby -> Flush -> Start"
                    : "Stop -> 3.5s Standby -> Start")
            : (usePauseFlush
                    ? "Pause -> 3.5s Idle -> Flush -> Start"
                    : "Stop -> 3.5s Idle -> Start");
    const int32_t sampleRate = mStream->getSampleRate();
    const int32_t channelCount = mStream->getChannelCount();
    const int32_t capacity = mStream->getBufferCapacityInFrames();
    const int32_t burst = mStream->getFramesPerBurst();
    const double capMs = framesToMs(capacity, sampleRate);
    const int64_t jumpThreshold = computeJumpThresholdFrames(sampleRate, burst, !isExclusive);
    const std::string streamDesc = formatString("%s %s (%d Hz, %dch, cap=%df/%.0fms, burst=%df)",
                                                dirStr, shareStr, sampleRate, channelCount,
                                                capacity, capMs, burst);

    appendTelemetryLine(formatString("  [Phase 3] %s (tol=+/-%lldf/%.1fms)",
                                     modeStr,
                                     static_cast<long long>(jumpThreshold),
                                     framesToMs(jumpThreshold, sampleRate)));

    auto recordStandbySetupFinding = [&](const std::string &tag, const std::string &detail) {
        *inoutAnyFailed = true;
        *inoutFailureTag = tag;
        Finding f;
        f.id = "STANDBY-STREAM-CONTROL-FAILURE";
        f.severity = "FAIL";
        f.title = "MMAP stream control call failed during 3.5s idle/standby check";
        f.whatHappened = "Starting, pausing, stopping, or flushing the MMAP stream before or after the 3.5s idle window returned an error or produced out-of-range timestamp ages.";
        f.androidContract = "AAudio.h: Stream state transitions across >3s idle windows (including EXCLUSIVE server MMAP standby) must succeed without returning error codes.";
        f.whyItMatters = "Preventing stream restart after multi-second idle breaks audio playback and multi-turn voice capture.";
        f.howToFix = "Ensure AAudioServiceStreamMMAP (for EXCLUSIVE standby) and the HAL cleanly close/recreate or resume the MMAP buffer without leaking resources or returning errors.";
        addOrUpdateFinding(f, streamDesc, detail);
    };

    mDataCallback->reset();
    if (mErrorCallback) mErrorCallback->reset();
    Result res = mStream->start(kStateChangeTimeoutNs);
    if (!mThreadEnabled.load()) return;
    if (checkAndReportDisconnect(streamDesc, "Phase 3 initial start", rowIndex, 0)) {
        *inoutDisconnected = true;
        return;
    }
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: initial start() returned %s", convertToText(res)));
        appendTelemetryLine("");
        recordStandbySetupFinding("FAIL (start failed)",
                                  formatString("%s %s (%s): initial start() returned %s.",
                                               dirStr, shareStr, modeStr, convertToText(res)));
        recordFail();
        return;
    }

    const int64_t cycleRunStartNs = AudioClock::getNanoseconds();
    if (!runUntilUnalignedPosition(0, 120, capacity, burst, jumpThreshold)) {
        if (checkAndReportDisconnect(streamDesc, "Phase 3 pre-idle run", rowIndex, 0)) {
            *inoutDisconnected = true;
        }
        return;
    }

    TimestampSample preSample{};
    Result lastTsRes = Result::OK;
    if (!waitForFreshTimestamp(cycleRunStartNs, 500, &preSample, &lastTsRes)) {
        if (!mThreadEnabled.load()) return;
        if (checkAndReportDisconnect(streamDesc, "Phase 3 pre-idle timestamp", rowIndex, 0)) {
            *inoutDisconnected = true;
            return;
        }
        appendTelemetryLine(formatString("    FAIL: no valid timestamp before idle (%s)",
                                         convertToText(lastTsRes)));
        appendTelemetryLine("");
        recordStandbySetupFinding("FAIL (pre-idle ts error)",
                                  formatString("%s %s (%s): no valid timestamp before 3.5s idle (%s).",
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
        res = mStream->pause(kStateChangeTimeoutNs);
    } else {
        res = mStream->stop(kStateChangeTimeoutNs);
    }
    if (!mThreadEnabled.load()) return;
    if (checkAndReportDisconnect(streamDesc, "Phase 3 pause/stop", rowIndex, 0)) {
        *inoutDisconnected = true;
        return;
    }
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: %s returned %s",
                                         usePauseFlush ? "pause()" : "stop()",
                                         convertToText(res)));
        appendTelemetryLine("");
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
    if (isExclusive) {
        appendTelemetryLine(formatString(
                "    Sleep  : Waiting %d ms for EXCLUSIVE MMAP server standby...",
                kStandbySleepMs));
    } else {
        appendTelemetryLine(formatString(
                "    Sleep  : Waiting %d ms idle (SHARED stays on service mixer; only EXCLUSIVE enters server standby)...",
                kStandbySleepMs));
    }

    if (!sleepMillisInterruptible(kStandbySleepMs)) {
        if (checkAndReportDisconnect(streamDesc, "Phase 3 3.5s idle wait", rowIndex, 0)) {
            *inoutDisconnected = true;
        }
        return;
    }

    if (usePauseFlush) {
        res = mStream->flush(kStateChangeTimeoutNs);
        if (!mThreadEnabled.load()) return;
        if (checkAndReportDisconnect(streamDesc, "Phase 3 post-idle flush", rowIndex, 0)) {
            *inoutDisconnected = true;
            return;
        }
        if (res != Result::OK) {
            appendTelemetryLine(formatString("    FAIL: flush() after idle returned %s",
                                             convertToText(res)));
            appendTelemetryLine("");
            recordStandbySetupFinding("FAIL (flush error)",
                                      formatString("%s %s (%s): flush() after 3.5s idle returned %s.",
                                                   dirStr, shareStr, modeStr, convertToText(res)));
            recordFail();
            return;
        }
    }

    const int64_t restartCallNs = AudioClock::getNanoseconds();
    res = mStream->start(kStateChangeTimeoutNs);
    if (!mThreadEnabled.load()) return;
    if (checkAndReportDisconnect(streamDesc, "Phase 3 post-idle start", rowIndex, 0)) {
        *inoutDisconnected = true;
        return;
    }
    if (res != Result::OK) {
        appendTelemetryLine(formatString("    FAIL: start() after idle returned %s",
                                         convertToText(res)));
        appendTelemetryLine("");
        recordStandbySetupFinding("FAIL (post-idle start)",
                                  formatString("%s %s (%s): start() after 3.5s idle returned %s.",
                                               dirStr, shareStr, modeStr, convertToText(res)));
        recordFail();
        return;
    }

    if (!sleepMillisInterruptible(200)) {
        if (checkAndReportDisconnect(streamDesc, "Phase 3 post-start run", rowIndex, 0)) {
            *inoutDisconnected = true;
        }
        return;
    }

    TimestampSample postSample{};
    if (!waitForFreshTimestamp(restartCallNs, 1000, &postSample, &lastTsRes)) {
        if (!mThreadEnabled.load()) return;
        if (checkAndReportDisconnect(streamDesc, "Phase 3 post-idle timestamp", rowIndex, 0)) {
            *inoutDisconnected = true;
            return;
        }
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
        appendTelemetryLine("    Result : FAIL");
        appendTelemetryLine("");

        *inoutAnyFailed = true;
        if (cbDeltaNow == 0) {
            *inoutFailureTag = "FAIL (0 callbacks on resume)";
            Finding fStall;
            fStall.id = (direction == Direction::Input)
                    ? "STANDBY-INPUT-ZERO-CALLBACKS"
                    : "STANDBY-OUTPUT-ZERO-CALLBACKS";
            fStall.severity = "FAIL";
            fStall.title = (direction == Direction::Input)
                    ? "Input MMAP stream delivers 0 audio callbacks after resuming from 3.5s idle/standby"
                    : "Output MMAP stream delivers 0 audio callbacks after resuming from 3.5s idle/standby";
            fStall.whatHappened = "After pausing/stopping for 3.5 seconds (triggering server MMAP standby on EXCLUSIVE streams) and calling start(), start() returned OK but onAudioReady() delivered 0 callbacks and DMA never advanced.";
            fStall.androidContract = "AAudio.h & StreamDescriptor.aidl: After an MMAP stream is idle for >3s and restarted via AAudioStream_requestStart(), hardware DMA and data callbacks must resume delivering frames.";
            fStall.whyItMatters = "Any audio app whose MMAP stream pauses or stops for >3 seconds and restarts receives 0 audio callbacks permanently (silent playback or frozen recording).";
            fStall.howToFix = "Possible causes:\n  1. EXCLUSIVE HAL standby exit sequence: AAudioServiceStreamMMAP reallocates the MMAP buffer (sending HAL 'start': STANDBY -> IDLE) before reconnecting the audio patch; if the HAL tries to start PCM during STANDBY -> IDLE before the backend DAI route is active and ignores the subsequent 'burst' command (IDLE -> ACTIVE), DMA never starts.\n  2. Client/service wakeup timer or mixer stall on resume.";
            addOrUpdateFinding(fStall, streamDesc,
                               formatString("%s %s (%s): After 3.5s idle and 1200 ms post-start wait, callback received cbDelta=+0 frames and appFrames remained stuck at %lldf.",
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
                    ? "Input getTimestamp() fails after resuming from 3.5s idle/standby"
                    : "Output getTimestamp() fails after resuming from 3.5s idle/standby";
            f.whatHappened = "After 3.5 seconds idle and start(), audio callbacks resumed transferring frames, but getTimestamp() failed to produce an updated timestamp within 1200 ms.";
            f.androidContract = "AAudio.h (AAudioStream_getTimestamp & AAudioStream_getFramesRead/Written): Must continue returning valid monotonic timestamps and frame counts after resuming from >3s idle/standby.";
            f.whyItMatters = "Audio apps that pause or stop an MMAP stream for >3 seconds and restart it lose timestamp reporting or see frozen client frame counters.";
            f.howToFix = "Possible causes:\n  1. AAudio client FIFO reallocation on EXCLUSIVE standby exit resetting new FIFO counters to 0 without carrying over the pre-standby frame offset.\n  2. HAL failing to report valid Reply.observable timestamps after standby exit.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s idle, callback transferred %+lld new frames (%.1f ms), appFrames=%lldf, and getTimestamp() failed with %s (%d).",
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

    int64_t preProjTs = 0;
    int64_t postProjTs = 0;
    double preAgeMs = 0.0;
    double postAgeMs = 0.0;
    const bool preAgeOk = projectTimestampToHostTime(preSample, sampleRate, &preProjTs, &preAgeMs);
    const bool postAgeOk = projectTimestampToHostTime(postSample, sampleRate, &postProjTs, &postAgeMs);
    if (!preAgeOk || !postAgeOk) {
        appendTelemetryLine(formatString(
                "    FAIL: out-of-range timestamp age (pre=%.1fms, post=%.1fms)",
                preAgeMs, postAgeMs));
        appendTelemetryLine("");
        recordStandbySetupFinding(
                "FAIL (stale ts age)",
                formatString("%s %s (%s): getTimestamp() age out of [-50ms, +250ms] bounds (pre=%.1f ms, post=%.1f ms).",
                             dirStr, shareStr, modeStr, preAgeMs, postAgeMs));
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
    const int64_t dTsPos = postProjTs - preProjTs;
    const int64_t dCb = postSample.callbackFrames - cbBeforeStandby;
    const int64_t dApp = (direction == Direction::Output) ? dWritten : dRead;
    const int64_t dHw = (direction == Direction::Output) ? dRead : dWritten;

    // Capacity-independent frozen-counter detection (~20 ms or >= 4 bursts of post-idle callbacks).
    const int64_t minExpectedPostCb = std::max<int64_t>(
            static_cast<int64_t>(burst) * 4, (static_cast<int64_t>(sampleRate) * 20) / 1000);
    const bool appCounterFrozen = (dCb >= minExpectedPostCb &&
                                   dApp <= std::max<int64_t>(burst, dCb / 4));
    const bool callbackStalled = (dCb == 0);
    const int64_t referenceAppDelta = (appCounterFrozen && dCb > 0) ? dCb : dApp;
    const int64_t tsError = dTsPos - referenceAppDelta;
    const int64_t dmaError = dHw - referenceAppDelta;

    const bool backwardTs = (postSample.tsPosition < preSample.tsPosition);
    const bool causalViolationOut = (direction == Direction::Output) &&
            ((postSample.tsPosition > postSample.framesWritten + burst) ||
             (postSample.framesRead > postSample.framesWritten + burst));
    const bool largeJump = (std::abs(tsError) > jumpThreshold) ||
            (std::abs(dmaError) > jumpThreshold);
    const bool checkFailed = callbackStalled || appCounterFrozen || backwardTs ||
            causalViolationOut || largeJump;

    appendTelemetryLine(formatString(
            "    After  : app=%lldf dma=%lldf ts=%lldf (cb=%+lldf)",
            static_cast<long long>(postApp),
            static_cast<long long>(postHw),
            static_cast<long long>(postSample.tsPosition),
            static_cast<long long>(dCb)));
    appendTelemetryLine(formatString(
            "    Jump   : tsErr=%+lldf(%+.1fms) dmaEr=%+lldf(%+.1fms)",
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
            "    Result : %s",
            checkFailed ? "FAIL" : "PASS"));
    appendTelemetryLine("");

    if (checkFailed) {
        *inoutAnyFailed = true;
        const int64_t classifyTol = std::max<int64_t>(burst * 3, (sampleRate * 6LL) / 1000);
        const int64_t preComp = (capacity > 0) ? (capacity - preRem) : 0;
        if (callbackStalled) {
            *inoutFailureTag = "FAIL (0 callbacks on resume)";
            Finding f;
            f.id = (direction == Direction::Input)
                    ? "STANDBY-INPUT-ZERO-CALLBACKS"
                    : "STANDBY-OUTPUT-ZERO-CALLBACKS";
            f.severity = "FAIL";
            f.title = (direction == Direction::Input)
                    ? "Input MMAP stream delivers 0 audio callbacks after resuming from 3.5s idle/standby"
                    : "Output MMAP stream delivers 0 audio callbacks after resuming from 3.5s idle/standby";
            f.whatHappened = "After 3.5 seconds idle and calling start(), start() returned OK but onAudioReady() delivered 0 callbacks.";
            f.androidContract = "AAudio.h & StreamDescriptor.aidl: After an MMAP stream resumes from >3s idle/standby via AAudioStream_requestStart(), hardware DMA and data callbacks must resume delivering frames.";
            f.whyItMatters = "Any audio app whose MMAP stream pauses or stops for >3 seconds and restarts receives 0 audio callbacks permanently.";
            f.howToFix = "Ensure the HAL starts (or retries starting) the MMAP PCM stream when transitioning to ACTIVE on 'burst' once the audio patch is reconnected after standby.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s idle and 200+ ms post-start wait, callback received cbDelta=+0 frames and appFrames remained at %lldf.",
                                            dirStr, shareStr, modeStr,
                                            static_cast<long long>(postApp)));
        } else if (appCounterFrozen || backwardTs ||
                   tsError < -jumpThreshold || dmaError < -jumpThreshold) {
            if (appCounterFrozen) {
                *inoutFailureTag = formatString("FAIL (app frozen, ts %+.0fms)",
                                                framesToMs(tsError, sampleRate));
            } else if (backwardTs) {
                *inoutFailureTag = formatString("FAIL (ts regressed %+.0fms)",
                                                framesToMs(tsError, sampleRate));
            } else {
                *inoutFailureTag = formatString("FAIL (ts jump %+.0fms)",
                                                framesToMs(tsError, sampleRate));
            }

            Finding f;
            f.id = (direction == Direction::Output)
                    ? "STANDBY-OUTPUT-POSITION-REGRESSION"
                    : "STANDBY-INPUT-POSITION-REGRESSION";
            f.severity = "FAIL";
            f.title = (direction == Direction::Output)
                    ? "Output frame counters freeze or regress after resuming from 3.5s idle/standby"
                    : "Input frame counters freeze or regress after resuming from 3.5s idle/standby";
            f.whatHappened = "After pausing or stopping for 3.5 seconds and calling start(), audio callbacks resumed transferring frames, but client getFramesWritten()/getFramesRead() froze near its pre-idle value or getTimestamp().position regressed/lagged relative to transferred frames.";
            f.androidContract = "AAudio.h & StreamDescriptor.aidl: Resuming from >3s idle/standby must preserve monotonic getFramesWritten(), getFramesRead(), and getTimestamp().position without resetting to 0 or freezing.";
            f.whyItMatters = "Causes client frame counters to freeze until post-standby transfer catches up or shifts timestamps backward by hundreds of milliseconds.";
            f.howToFix = "Possible causes:\n  1. AAudio client FIFO reallocation on EXCLUSIVE standby exit resetting new FIFO counters to 0 without carrying over the pre-standby frame total (or server roundUp64(capacity) pushing mFramesOffsetFromService negative so monotonic clamping holds getTimestamp().position flat).\n  2. HAL resetting cumulative MMAP frame counters or clearing its pre-stop position latch asymmetrically across standby.";
            if (appCounterFrozen) {
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): Callback transferred %+lld new frames (%.1f ms) after 3.5s idle, but app counter advanced only %lldf -> %lldf and tsPosition moved %lldf -> %lldf (tsError=%+lldf / %+.1f ms).",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(dCb),
                                                framesToMs(dCb, sampleRate),
                                                static_cast<long long>(preApp),
                                                static_cast<long long>(postApp),
                                                static_cast<long long>(preSample.tsPosition),
                                                static_cast<long long>(postSample.tsPosition),
                                                static_cast<long long>(tsError),
                                                framesToMs(tsError, sampleRate)));
            } else if (backwardTs) {
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): After 3.5s idle, getTimestamp().position moved BACKWARD from %lldf to %lldf (tsError=%+lldf / %+.1f ms) while appFrames advanced %lldf -> %lldf.",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(preSample.tsPosition),
                                                static_cast<long long>(postSample.tsPosition),
                                                static_cast<long long>(tsError),
                                                framesToMs(tsError, sampleRate),
                                                static_cast<long long>(preApp),
                                                static_cast<long long>(postApp)));
            } else {
                addOrUpdateFinding(f, streamDesc,
                                   formatString("%s %s (%s): After 3.5s idle, getTimestamp().position lagged behind appFrames by %+lld frames (%+.1f ms), advancing %lldf -> %lldf while appFrames advanced %lldf -> %lldf.",
                                                dirStr, shareStr, modeStr,
                                                static_cast<long long>(tsError),
                                                framesToMs(tsError, sampleRate),
                                                static_cast<long long>(preSample.tsPosition),
                                                static_cast<long long>(postSample.tsPosition),
                                                static_cast<long long>(preApp),
                                                static_cast<long long>(postApp)));
            }
        } else if (preRem > 0 &&
                   (std::abs(tsError - preRem) <= classifyTol ||
                    std::abs(dmaError - preRem) <= classifyTol ||
                    (preComp > 0 && (std::abs(tsError - preComp) <= classifyTol ||
                                     std::abs(dmaError - preComp) <= classifyTol)))) {
            *inoutFailureTag = "FAIL (ring offset jump)";
            Finding f;
            f.id = "STANDBY-UNALIGNED-RING-OFFSET";
            f.severity = "FAIL";
            f.title = "MMAP frame position jumps by unaligned ring-buffer offset after 3.5s idle/standby";
            f.whatHappened = "After resuming from 3.5 seconds idle/standby, reported frame positions shifted by the unaligned ring-buffer stop offset (rem) or its complement (capacity - rem).";
            f.androidContract = "AAudio.h: Resuming after >3s idle/standby must maintain continuous frame and presentation counters without leaking ring-buffer alignment offsets.";
            f.whyItMatters = "Causes forward timestamp jumps and false XRun buffer inflation whenever an audio stream resumes after >3s idle.";
            f.howToFix = "Inspect both AAudio client/server standby offset accounting (roundUp64(capacity) vs client FIFO recreation) and HAL standby position alignment when the hardware ring buffer resets to index 0.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s idle (stop@ringOffset=%lld/%df), jumped by tsError=%+lldf (%+.1f ms), dmaError=%+lldf (%+.1f ms).",
                                            dirStr, shareStr, modeStr,
                                            static_cast<long long>(preRem),
                                            capacity,
                                            static_cast<long long>(tsError),
                                            framesToMs(tsError, sampleRate),
                                            static_cast<long long>(dmaError),
                                            framesToMs(dmaError, sampleRate)));
        } else {
            *inoutFailureTag = formatString("FAIL (ts jump %+.0fms)",
                                            framesToMs(tsError, sampleRate));
            Finding f;
            f.id = "STANDBY-POSITION-JUMP";
            f.severity = "FAIL";
            f.title = "MMAP frame position or opposite-end counter jumps forward after 3.5s idle/standby";
            f.whatHappened = "After resuming from 3.5 seconds idle/standby, projected getTimestamp() or opposite-end counters jumped forward beyond tolerance relative to transferred frames.";
            f.androidContract = "AAudio.h & StreamDescriptor.aidl: Resuming after >3s idle/standby must maintain continuous hardware and presentation frame counts matching actual frames transferred.";
            f.whyItMatters = "Causes forward timestamp jumps, A/V lip-sync offset, and false XRun buffer inflation whenever an audio stream resumes after >3s idle.";
            f.howToFix = "Inspect how AAudioServiceEndpointMMAP and the HAL preserve and rebase cumulative MMAP frame counters across standby entry and exit so no extra buffer capacity or offset is added on resume.";
            addOrUpdateFinding(f, streamDesc,
                               formatString("%s %s (%s): After 3.5s idle (stop@ringOffset=%lld/%df), jumped forward by tsError=%+lldf (%+.1f ms), dmaError=%+lldf (%+.1f ms).",
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
