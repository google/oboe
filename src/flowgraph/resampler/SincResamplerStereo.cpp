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

#include "SincResamplerStereo.h"

using namespace resampler;

#define STEREO  2

SincResamplerStereo::SincResamplerStereo(const MultiChannelResampler::Builder &builder)
        : SincResampler(builder) {
    assert(builder.getChannelCount() == STEREO);
}

void SincResamplerStereo::writeFrame(const float *frame) {
    int xIndex = mCursor * STEREO;
    int offset = kNumTaps * STEREO;
    float *dest = &mX[xIndex];
        // Write each channel twice so we avoid having to wrap when running the FIR.
    dest[0] = dest[offset] = frame[0];
    dest[1] = dest[1 +  offset] = frame[1];
    if (++mCursor >= kNumTaps) {
        mCursor = 0;
    }
}

void SincResamplerStereo::readFrame(float *frame) {
    float left = 0.0;
    float right = 0.0;
    float phase =  getPhase();
    // Multiply input times windowed sinc function.
    int xIndex = (mCursor + kNumTaps) * STEREO;
    for (int i = 0; i < kNumTaps; i++) {
        float coefficient = interpolateWindowedSinc(phase);
        float *xFrame = &mX[xIndex];
        left += coefficient * xFrame[0];
        right += coefficient * xFrame[1];
        xIndex -= STEREO;
        phase += 1.0;
    }
    // Copy accumulator to output.
    frame[0] = left;
    frame[1] = right;
}
