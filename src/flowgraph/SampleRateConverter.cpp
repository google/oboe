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
#include "SampleRateConverter.h"

using namespace flowgraph;

SampleRateConverter::SampleRateConverter(int32_t channelCount)
        : input(*this, channelCount)
        , output(*this, channelCount)
        , mResampler(channelCount) {
    setDataPulledAutomatically(false);
}

int32_t SampleRateConverter::checkInputFrames() {
    if (mInputCursor >= mInputValid) {
        mInputValid = input.pullData(mInputFramePosition, input.getFramesPerBuffer());
        LOGD("SampleRateConverter::checkInputFrames: pulled %d", mInputValid);
        mInputFramePosition += mInputValid;
        mInputCursor = 0;
    }
    return mInputValid;
}

const float *SampleRateConverter::getNextInputFrame() {
    const float *inputBuffer = input.getBuffer();
    return &inputBuffer[mInputCursor++ * input.getSamplesPerFrame()];
}

int32_t SampleRateConverter::onProcess(int32_t numFrames) {
    float *outputBuffer = output.getBuffer();
    int32_t channelCount = output.getSamplesPerFrame();

    int framesLeft = numFrames;
    while (framesLeft > 0) {
        // Gather input samples as needed.
        while(mPhase >= 1.0) {
            if (checkInputFrames() <= 0) {
                break;
            }
            mPhase -= 1.0;
            const float *frame = getNextInputFrame();
            mResampler.writeFrame(frame);
        }

        // If phase >= 1.0 then we are waiting for input data.
        if (mPhase < 1.0) {
            // Output frame is interpolated from input samples based on phase.
            mResampler.readFrame(outputBuffer, mPhase);
            mPhase += mPhaseIncrement;
            outputBuffer += channelCount;
            framesLeft--;
        } else {
            break;
        }
    }
    return numFrames - framesLeft;
}

