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

MultiChannelResampler::MultiChannelResampler(int32_t numTaps, int32_t channelCount)
        : mNumTaps(numTaps)
        , mX(channelCount * numTaps * 2)
        , mSingleFrame(channelCount)
        , mChannelCount(channelCount)
        {}


MultiChannelResampler *MultiChannelResampler::make(int32_t channelCount,
                                                   int32_t inputRate,
                                                   int32_t outputRate,
                                                   Quality quality) {
    int numTaps = 4;
    switch (quality) {
        case Quality::Low: // TODO polynomial?
            return new LinearResampler(inputRate, outputRate, channelCount);
        case Quality::Medium:
            numTaps = 12; // TODO benchmark and review these numTaps
            break;
        case Quality::High:
        default:
            numTaps = 20;
            break;
        case Quality::Best:
            numTaps = 28;
            break;
    }

    IntegerRatio ratio(inputRate, outputRate);
    ratio.reduce();
    bool usePolyphase = (numTaps * ratio.getDenominator()) <= kMaxCoefficients;

    if (usePolyphase) {
        if (channelCount == 1) {
            return new PolyphaseResamplerMono(numTaps, inputRate, outputRate);
        } else if (channelCount == 2) {
            return new PolyphaseResamplerStereo(numTaps, inputRate, outputRate);
        } else {
            return new PolyphaseResampler(numTaps, inputRate, outputRate, channelCount);
        }
    } else {
        // Use less optimized resampler that uses a float phaseIncrement.
        // TODO mono resampler
        if (channelCount == 2) {
            return new SincResamplerStereo(inputRate, outputRate); // TODO pass spread
        } else {
            return new SincResampler(inputRate, outputRate, channelCount); // TODO pass spread
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
        // Write twice so we avoid having to wrap when reading.
        dest[channel] = dest[channel + offset] = frame[channel];
    }
}

float MultiChannelResampler::hammingWindow(float radians, int spread) {
    const float alpha = 0.54f;
    const float windowPhase = radians / spread;
    return (float) (alpha + ((1.0 - alpha) * cosf(windowPhase)));
}

// Unoptimized calculation used to construct lookup tables.
float MultiChannelResampler::calculateWindowedSinc(float radians, int spread) {
    if (abs(radians) < 0.00000001) return 1.0f;   // avoid divide by zero
    const float sinc = sinf(radians) / radians;   // Sinc function
    return sinc * hammingWindow(radians, spread); // TODO try Kaiser window
}
