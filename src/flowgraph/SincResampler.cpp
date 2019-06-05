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

using namespace flowgraph;

SincResampler::SincResampler(int32_t channelCount)
        : MultiChannelResampler(channelCount)
        , mX(channelCount * kNumTaps * 2)
        , mSingleFrame(channelCount)
        , mWindowedSinc(kNumPoints + kNumGuardPoints){
    generateLookupTable();
}

void SincResampler::writeFrame(const float *frame) {
    int xIndex = mCursor * getChannelCount();
    int offset = kNumTaps * getChannelCount();
    float *dest = &mX[xIndex];
    for (int channel = 0; channel < getChannelCount(); channel++) {
        // Write twice so we avoid having to wrap when running the FIR.
        dest[channel] = dest[channel + offset] = frame[channel];
    }
    if (++mCursor >= kNumTaps) {
        mCursor = 0;
    }
}

void SincResampler::readFrame(float *frame, float phase) {
    // Clear accumulator for mix.
    for (int channel = 0; channel < getChannelCount(); channel++) {
        mSingleFrame[channel] = 0.0;
    }
    // Multiply input times windowed sinc function.
    int xIndex = (mCursor + kNumTaps) * getChannelCount();
    for (int i = 0; i < kNumTaps; i++) {
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

float SincResampler::interpolateWindowedSinc(float phase) {
    float tablePhase = phase * kTablePhaseScaler;
    int tableIndex = int(tablePhase);
    float fraction = tablePhase - tableIndex;
    float low = mWindowedSinc[tableIndex];
    float high = mWindowedSinc[tableIndex + 1]; // OK because of guard point
    return (float) (low + (fraction * (high - low)));
}

float SincResampler::calculateWindowedSinc(float phase) {
    const float realPhase = phase - kSpread;
    if (abs(realPhase) < 0.00000001) return 1.0f; // avoid divide by zero
    // Hamming window TODO try Kaiser window
    const float alpha = 0.54f;
    const float windowPhase = realPhase * M_PI * kSpreadInverse;
    const float window = (float) (alpha + ((1.0 - alpha) * cosf(windowPhase)));
    const float sincPhase = realPhase * M_PI;
    const float sinc = sinf(sincPhase) / sincPhase;
    return window * sinc;
}

void SincResampler::generateLookupTable() {
    // TODO store only half of function and use symmetry
    for (int i = 0; i < mWindowedSinc.size(); i++) {
        float phase = (i * 2.0 * kSpread) / kNumPoints;
        mWindowedSinc[i] = calculateWindowedSinc(phase);
    }
}
