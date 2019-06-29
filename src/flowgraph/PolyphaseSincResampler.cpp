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

#include "IntegerRatio.h"
#include "PolyphaseSincResampler.h"

using namespace flowgraph;

PolyphaseSincResampler::PolyphaseSincResampler(int32_t channelCount,
                             int32_t inputRate,
                             int32_t outputRate)
        : MultiChannelResampler(channelCount, kNumTaps, inputRate, outputRate)
        {
    generateCoefficients(inputRate, outputRate);
}

void PolyphaseSincResampler::generateCoefficients(int32_t inputRate, int32_t outputRate) {
    IntegerRatio ratio(inputRate, outputRate);
    ratio.reduce();
    mNumerator = ratio.getNumerator();
    mDenominator = ratio.getDenominator();
    mIntegerPhase = mDenominator;
    mCoefficients.resize(getNumTaps() * ratio.getDenominator());
    int cursor = 0;
    double phase = 0.0;
    double phaseIncrement = (double) inputRate / (double) outputRate;
    for (int i = 0; i < ratio.getDenominator(); i++) {
        float tapPhase = phase;
        for (int tap = 0; tap < getNumTaps(); tap++) {
            mCoefficients.at(cursor++) = calculateWindowedSinc(tapPhase, kSpread);
            tapPhase += 1.0;
        }
        phase += phaseIncrement;
        while (phase >= 1.0) {
            phase -= 1.0;
        }
    }
}
void PolyphaseSincResampler::readFrame(float *frame) {
    // Clear accumulator for mix.
    for (int channel = 0; channel < getChannelCount(); channel++) {
        mSingleFrame[channel] = 0.0;
    }

    float *coefficients = &mCoefficients[mCoefficientCursor];
    // Multiply input times windowed sinc function.
    int xIndex = (mCursor + kNumTaps) * getChannelCount();
    for (int i = 0; i < kNumTaps; i++) {
        float coefficient = *coefficients++;
        float *xFrame = &mX[xIndex];
        for (int channel = 0; channel < getChannelCount(); channel++) {
            mSingleFrame[channel] += coefficient * xFrame[channel];
        }
        xIndex -= getChannelCount();
    }

    mCoefficientCursor += kNumTaps;
    if (mCoefficientCursor >= mCoefficients.size()) {
        mCoefficientCursor = 0;
    }

    // Copy accumulator to output.
    for (int channel = 0; channel < getChannelCount(); channel++) {
        frame[channel] = mSingleFrame[channel];
    }
}
