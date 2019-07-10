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
#include "PolyphaseResamplerMono.h"
#include "PolyphaseResamplerStereo.h"
#include "SincResampler.h"
#include "SincResamplerStereo.h"

using namespace flowgraph;

MultiChannelResampler::MultiChannelResampler(int32_t channelCount,
        int32_t numTaps,
        int32_t inputRate,
        int32_t outputRate)
        : mChannelCount(channelCount)
        , mNumTaps(numTaps)
        , mX(channelCount * getNumTaps() * 2)
        , mSingleFrame(channelCount)
        {}


MultiChannelResampler *MultiChannelResampler::make(int32_t channelCount,
                                                   int32_t inputRate,
                                                   int32_t outputRate,
                                                   Quality quality) {
    bool usePolyphase = true;
    int numTaps = 4;
    switch (quality) {
        case Quality::Low: // TODO polynomial
            return new LinearResampler(channelCount, inputRate, outputRate);
        case Quality::Medium:
        default:
            numTaps = 12;
            break;
        case Quality::High:
            numTaps = 20;
            break;
        case Quality::Best:
            usePolyphase = false; // TODO base on IntegerRatio reduction
            break;
    }

    if (usePolyphase) {
        if (channelCount == 1) {
            return new PolyphaseResamplerMono(numTaps, inputRate, outputRate);
        } else if (channelCount == 2) {
            return new PolyphaseResamplerStereo(numTaps, inputRate, outputRate);
        } else {
            return new PolyphaseResampler(numTaps, channelCount, inputRate, outputRate);
        }
    } else {
        // TODO mono resampler
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

// Unoptimized calculation used to construct lookup tables.
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
