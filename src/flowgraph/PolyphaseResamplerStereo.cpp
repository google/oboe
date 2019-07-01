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

#include "PolyphaseResamplerStereo.h"

using namespace flowgraph;

#define STEREO  2

PolyphaseResamplerStereo::PolyphaseResamplerStereo(
                                       int32_t inputRate,
                                       int32_t outputRate)
        : PolyphaseResampler(STEREO, inputRate, outputRate) {}


void PolyphaseResamplerStereo::writeFrame(const float *frame) {
    float *dest = &mX[mCursor * STEREO];
    // Write each channel twice so we avoid having to wrap when running the FIR.
    const int offset = kNumTaps * STEREO;
    const float left =  frame[0];
    const float right = frame[1];
    // Put ordered writes together.
    dest[0] = left;
    dest[1] = right;
    dest[offset] = left;
    dest[1 +  offset] = right;
    if (++mCursor >= kNumTaps) {
        mCursor = 0;
    }
}

void PolyphaseResamplerStereo::readFrame(float *frame) {
    // Clear accumulators.
    float left = 0.0;
    float right = 0.0;

    // Multiply input times precomputed windowed sinc function.
    const float *coefficients = &mCoefficients[mCoefficientCursor];
    int xIndex = (mCursor + kNumTaps) * STEREO;
    for (int i = 0; i < kNumTaps; i++) {
        float coefficient = *coefficients++;
        float *xFrame = &mX[xIndex];
        left += coefficient * xFrame[0];
        right += coefficient * xFrame[1];
        xIndex -= STEREO;
    }

    mCoefficientCursor = (mCoefficientCursor + kNumTaps) % mCoefficients.size();

    // Copy accumulators to output.
    frame[0] = left;
    frame[1] = right;
}
