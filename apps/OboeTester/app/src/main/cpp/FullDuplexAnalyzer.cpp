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

#include "common/OboeDebug.h"
#include "FullDuplexAnalyzer.h"

oboe::Result  FullDuplexAnalyzer::start() {
    getLoopbackProcessor()->setSampleRate(getOutputStream()->getSampleRate());
    getLoopbackProcessor()->onStartTest();
    return FullDuplexStream::start();
}

oboe::DataCallbackResult FullDuplexAnalyzer::onBothStreamsReady(
        const void *inputData,
        int   numInputFrames,
        void *outputData,
        int   numOutputFrames) {

    int32_t inputStride = getInputStream()->getChannelCount();
    int32_t outputStride = getOutputStream()->getChannelCount();
    float *inputFloat = (float *) inputData;
    float *outputFloat = (float *) outputData;

    (void) getLoopbackProcessor()->process(inputFloat, inputStride, numInputFrames,
                                   outputFloat, outputStride, numOutputFrames);

    // write the first channel of output and input to the stereo recorder
    if (mRecording != nullptr) {
        float buffer[2];
        int numBoth = std::min(numInputFrames, numOutputFrames);
        for (int i = 0; i < numBoth; i++) {
            buffer[0] = *outputFloat;
            outputFloat += outputStride;
            buffer[1] = *inputFloat;
            inputFloat += inputStride;
            mRecording->write(buffer, 1);
        }
        // Handle mismatch in in numFrames.
        buffer[0] = 0.0f; // gap in output
        for (int i = numBoth; i < numInputFrames; i++) {
            buffer[1] = *inputFloat;
            inputFloat += inputStride;
            mRecording->write(buffer, 1);
        }
        buffer[1] = 0.0f; // gap in input
        for (int i = numBoth; i < numOutputFrames; i++) {
            buffer[0] = *outputFloat;
            outputFloat += outputStride;
            mRecording->write(buffer, 1);
        }
    }
    return oboe::DataCallbackResult::Continue;
};
