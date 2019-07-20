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

#include <math.h>
#include "SincResampler.h"

using namespace resampler;

SincResampler::SincResampler(const MultiChannelResampler::Builder &builder)
        : ContinuousResampler(builder)
        , mWindowedSinc(kNumPoints + kNumGuardPoints) {
    generateLookupTable();
}


void SincResampler::readFrame(float *frame) {
    // Clear accumulator for mix.
    for (int channel = 0; channel < getChannelCount(); channel++) {
        mSingleFrame[channel] = 0.0;
    }
    float phase =  getPhase();
    // Multiply input times windowed sinc function.
    int xIndex = (mCursor + kNumTaps) * getChannelCount();
    for (int i = 0; i < kNumTaps; i++) {
        // TODO Use consecutive coefficient precomputed and then interpolate the result.
        float coefficient = interpolateWindowedSinc(phase);
        float *xFrame = &mX[xIndex];
        for (int channel = 0; channel < getChannelCount(); channel++) {
            mSingleFrame[channel] += coefficient * xFrame[channel];
        }
        xIndex -= getChannelCount();
        phase += 1.0;
    }
    // Copy accumulator to output.
    for (int channel = 0; channel < getChannelCount(); channel++) {
        frame[channel] = mSingleFrame[channel];
    }
}
#if 0
static inline uint16_t fast_int_from_float(float f)
{
    // Offset is used to expand the valid range of [-1.0, 1.0) into the 16 lsbs of the
    // floating point significand.
    static const float offset = (float)(3 << 22);
    union {
        float f;
        int32_t i;
    } u;
    u.f = (f - 0.5) + offset; // recenter valid range
    return u.i & 0x0FFFF; // Return lower 16 bits, the part of interest in the significand.
}
#endif

float SincResampler::interpolateWindowedSinc(float phase) {
    // convert from 0 to 2*kSpread range to 0 to table size range
    float tablePhase = phase * kTablePhaseScaler;
    //uint16_t tableIndex = fast_int_from_float(tablePhase);
    long tableIndex = lrintf(tablePhase);
    //int tableIndex = int(tablePhase);
    float low = mWindowedSinc[tableIndex];
    float high = mWindowedSinc[tableIndex + 1]; // OK because of guard point
    float fraction = tablePhase - tableIndex;
    return low + (fraction * (high - low));
}

void SincResampler::generateLookupTable() {
    // Place related coefficients together for faster convolution, like for Polyphase
    // but divide each zero crossing into 32 or 64 steps.
    // TODO store only half of function and use symmetry
    // By iterating over the table size we also set the guard point.
    for (int i = 0; i < mWindowedSinc.size(); i++) {
        float phase = (i * 2.0 * kSpread) / kNumPoints;
        float radians = (phase - kSpread) * M_PI;
        mWindowedSinc[i] = calculateWindowedSinc(radians, kSpread);
    }
}
