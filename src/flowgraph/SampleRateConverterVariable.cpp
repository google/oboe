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

#include "SampleRateConverterVariable.h"

using namespace flowgraph;

SampleRateConverterVariable::SampleRateConverterVariable(int32_t channelCount, MultiChannelResampler &resampler)
        : SampleRateConverterVariable(channelCount, resampler) {
}

int32_t SampleRateConverterVariable::onProcess(int32_t numFrames) {
    float *outputBuffer = output.getBuffer();
    int32_t channelCount = output.getSamplesPerFrame();
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        // Gather input samples as needed.
        if(mResampler.isWriteReady()) {
            if (!isInputAvailable()) break;
            const float *frame = getNextInputFrame();
            mResampler.writeFrame(frame);
        }

        // If phase >= 1.0 then we are waiting for input data.
        if (mResampler.isReadReady()) {
            // Output frame is interpolated from input samples based on phase.
            mResampler.readFrame(outputBuffer);
            outputBuffer += channelCount;
            framesLeft--;
        } else {
            break;
        }
    }
    return numFrames - framesLeft;
}
