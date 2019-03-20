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

oboe::Result  FullDuplexLatency::start() {
    mEchoAnalyzer.reset();
    return FullDuplexStream::start();
}

static void analyze_data(FullDuplexLatency *fullDuplexLatency) {
    fullDuplexLatency->analyzeData();
}

oboe::DataCallbackResult FullDuplexLatency::onBothStreamsReady(
        const void *inputData,
        int   numInputFrames,
        void *outputData,
        int   numOutputFrames) {
    // reset analyzer if we miss some input data
    if (numInputFrames < numOutputFrames) {
        LOGD("numInputFrames (%4d) < numOutputFrames (%4d) so reset analyzer",
             numInputFrames, numOutputFrames);
        mEchoAnalyzer.reset();
    } else {
        float *inputFloat = (float *) inputData;
        float *outputFloat = (float *) outputData;
        int32_t outputStride = getOutputStream()->getChannelCount();

        (void) mEchoAnalyzer.process(inputFloat, getInputStream()->getChannelCount(),
                                     outputFloat, outputStride,
                                     numOutputFrames);

        // zero out remainder of output array
        int32_t framesLeft = numOutputFrames - numInputFrames;
        outputFloat += numOutputFrames * outputStride;

        if (framesLeft > 0) {
            memset(outputFloat, 0, framesLeft * getOutputStream()->getBytesPerFrame());
        }
    }
    // Are we done?
    if (mEchoAnalyzer.hasEnoughData()) {
        // Crunch the numbers on a separate thread.
        std::thread t(analyze_data, this);
        t.detach();
        return oboe::DataCallbackResult::Stop;
    } else {
        return oboe::DataCallbackResult::Continue;
    }
};
