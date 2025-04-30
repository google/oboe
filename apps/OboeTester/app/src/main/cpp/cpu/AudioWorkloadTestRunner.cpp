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

#include "AudioWorkloadTestRunner.h"
#include <iostream>
#include <iomanip>
#include <sstream>

AudioWorkloadTestRunner::~AudioWorkloadTestRunner() {
    stop();
}

int32_t AudioWorkloadTestRunner::start(
        int32_t targetDurationMs,
        int32_t bufferSizeInBursts,
        int32_t numVoices,
        int32_t highNumVoices,
        int32_t highLowPeriodMillis,
        bool adpfEnabled,
        bool hearWorkload) {
    if (mIsRunning) {
        std::cerr << "Error: Test already running." << std::endl;
        return -1;
    }

    if (mAudioWorkloadTest.open() != static_cast<int32_t>(oboe::Result::OK)) {
        mResultText = "Error opening audio stream.";
        mResult = -1;
        mIsDone = true;
        return -1;
    }

    mIsRunning = true;
    mIsDone = false;
    mResult = 0;
    mResultText = "Running...";

    int32_t result = mAudioWorkloadTest.start(
            targetDurationMs,
            bufferSizeInBursts,
            numVoices,
            highNumVoices,
            highLowPeriodMillis,
            adpfEnabled,
            hearWorkload);

    if (result != static_cast<int32_t>(oboe::Result::OK)) {
        mResultText = "Error starting audio stream: ";
        mResultText += oboe::convertToText(static_cast<oboe::Result>(result));
        mResult = -1;
        mIsDone = true;
        mIsRunning = false;
        mAudioWorkloadTest.close();
        return -1;
    }

    return 0;
}

bool AudioWorkloadTestRunner::stopIfDone() {
    if (!mIsDone && !mAudioWorkloadTest.isRunning()) {
        stop();
    }
    return mIsDone;
}

std::string AudioWorkloadTestRunner::getStatus() const {
    if (!mIsRunning) {
        return mResultText;
    }
    int32_t callbacksCompleted = mAudioWorkloadTest.getCallbackCount();
    return "Running: " + std::to_string(callbacksCompleted)
            + " callbacks completed. XRuns: " + std::to_string(mAudioWorkloadTest.getXRunCount());
}

int32_t AudioWorkloadTestRunner::stop() {
    if (mIsRunning) {
        mAudioWorkloadTest.stop();
        mAudioWorkloadTest.close();
        mIsRunning = false;

        int32_t xRunCount = mAudioWorkloadTest.getXRunCount();
        if (xRunCount > 0) {
            mResult = -1;
            mResultText = "FAIL: Encountered " + std::to_string(xRunCount) + " xruns.";
        } else {
            mResult = 1;
            mResultText = "PASS: No xruns encountered.";
        }
        mIsDone = true;
        return 0;
    }
    return -1; // Not running
}

int32_t AudioWorkloadTestRunner::getResult() const {
    return mResult;
}

std::string AudioWorkloadTestRunner::getResultText() const {
    return mResultText;
}
