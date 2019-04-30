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
    getLoopbackProcessor()->reset();
    return FullDuplexStream::start();
}

oboe::DataCallbackResult FullDuplexAnalyzer::onBothStreamsReady(
        const void *inputData,
        int   numInputFrames,
        void *outputData,
        int   numOutputFrames) {

    int32_t inputStride = getInputStream()->getChannelCount();
    int32_t outputStride = getOutputStream()->getChannelCount();

    // TODO Pull up into superclass
    // reset analyzer if we miss some input data
    if (numInputFrames < numOutputFrames) {
        LOGD("numInputFrames (%4d) < numOutputFrames (%4d) so reset analyzer",
             numInputFrames, numOutputFrames);
        getLoopbackProcessor()->reset();
    } else {
        float *inputFloat = (float *) inputData;
        float *outputFloat = (float *) outputData;

        (void) getLoopbackProcessor()->process(inputFloat, inputStride,
                                       outputFloat, outputStride,
                                       numOutputFrames);

        // zero out remainder of output array
        int32_t framesLeft = numOutputFrames - numInputFrames;
        outputFloat += numOutputFrames * outputStride;

        if (framesLeft > 0) {
            memset(outputFloat, 0, framesLeft * getOutputStream()->getBytesPerFrame());
        }
    }

    // write the first channel of output and input to the recorder
    if (mRecording != nullptr) {
        float buffer[2];
        float *inputFloat = (float *) inputData;
        float *outputFloat = (float *) outputData;
        for (int i = 0; i < numOutputFrames; i++) {
            buffer[0] = *outputFloat;
            outputFloat += outputStride;
            if (i < numInputFrames) {
                buffer[1] = *inputFloat;
                inputFloat += inputStride;
            } else {
                buffer[1] = 0.0f;
            }
            mRecording->write(buffer, 1);
        }
    }
    return oboe::DataCallbackResult::Continue;
};
