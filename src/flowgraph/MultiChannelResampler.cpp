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

#include "IntegerRatio.h"
#include "LinearResampler.h"
#include "MultiChannelResampler.h"
#include "PolyphaseResampler.h"
#include "PolyphaseResamplerStereo.h"
#include "SincResampler.h"
#include "SincResamplerStereo.h"

using namespace flowgraph;

MultiChannelResampler *MultiChannelResampler::make(int32_t channelCount,
                                                   int32_t inputRate,
                                                   int32_t outputRate,
                                                   Quality quality) {
    switch (quality) {
        case Quality::Low:
        case Quality::Medium: // TODO polynomial
            return new LinearResampler(channelCount, inputRate, outputRate);
        default:
        case Quality::High:
            // TODO mono resampler
            if (channelCount == 2) {
                return new PolyphaseResamplerStereo(inputRate, outputRate);
            } else {
                return new PolyphaseResampler(channelCount, inputRate, outputRate);
            }
        case Quality::Best:
            if (channelCount == 2) {
                return new SincResamplerStereo( inputRate, outputRate); // TODO pass spread
            } else {
                return new SincResampler(channelCount,  inputRate, outputRate); // TODO pass spread
            }
    }
}


void MultiChannelResampler::writeFrame(const float *frame) {
    // Advance cursor before write so that cursor points to last written frame in read.
    if (++mCursor >= getNumTaps()) {
        mCursor = 0;
    }
    float *dest = &mX[mCursor * getChannelCount()];
    int offset = getNumTaps() * getChannelCount();
    for (int channel = 0; channel < getChannelCount(); channel++) {
        // Write twice so we avoid having to wrap when running the FIR.
        dest[channel] = dest[channel + offset] = frame[channel];
    }
}


float MultiChannelResampler::calculateWindowedSinc(float phase, int spread) {
    const float realPhase = phase - spread;
    if (abs(realPhase) < 0.00000001) return 1.0f; // avoid divide by zero
    // Hamming window TODO try Kaiser window
    const float alpha = 0.54f;
    const float windowPhase = realPhase * M_PI / spread;
    const float window = (float) (alpha + ((1.0 - alpha) * cosf(windowPhase)));
    // Sinc function
    const float sincPhase = realPhase * M_PI;
    const float sinc = sinf(sincPhase) / sincPhase;
    return window * sinc;
}
