/*
 * Copyright 2019 The Android Open Source Project
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

#include <thread>

#include "common/OboeDebug.h"
#include "FullDuplexLatency.h"

static void analyze_data(FullDuplexLatency *fullDuplexLatency) {
    fullDuplexLatency->analyzeData();
}

oboe::DataCallbackResult FullDuplexLatency::onBothStreamsReady(
        const void *inputData,
        int   numInputFrames,
        void *outputData,
        int   numOutputFrames) {

    oboe::DataCallbackResult callbackResult = FullDuplexAnalyzer::onBothStreamsReady(
            inputData, numInputFrames, outputData, numOutputFrames);

    // Are we done?
    if (mEchoAnalyzer.hasEnoughData()) {
        // Crunch the numbers on a separate thread.
        std::thread t(analyze_data, this);
        t.detach();
        callbackResult = oboe::DataCallbackResult::Stop;
    }

    return callbackResult;
};
