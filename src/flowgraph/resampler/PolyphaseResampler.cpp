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
#include "PolyphaseResampler.h"

using namespace resampler;

PolyphaseResampler::PolyphaseResampler(const MultiChannelResampler::Builder &builder)
        : MultiChannelResampler(builder)
        {
    assert((getNumTaps() % 4) == 0); // Required for loop unrolling.
    generateCoefficients(builder.getInputRate(), builder.getOutputRate(), builder.getNormalizedCutoff());
}

// Generate coefficients in the order they will be used by readFrame().
// This is more complicated but readFrame() is called repeatedly and should be optimized.
void PolyphaseResampler::generateCoefficients(int32_t inputRate,
                                              int32_t outputRate,
                                              float normalizedCutoff) {
    {
        // Reduce sample rates to the smallest ratio.
        // For example 44100/48000 would become 147/160.
        IntegerRatio ratio(inputRate, outputRate);
        ratio.reduce();
        mNumerator = ratio.getNumerator();
        mDenominator = ratio.getDenominator();
        mIntegerPhase = mDenominator;
        mCoefficients.resize(getNumTaps() * ratio.getDenominator());
    }
    int cursor = 0;
    double phase = 0.0;
    double phaseIncrement = (double) inputRate / (double) outputRate;
    const int spread = getNumTaps() / 2; // numTaps must be even.
    for (int i = 0; i < mDenominator; i++) {
        float tapPhase = phase - spread;
        float gain = 0.0;
        int gainCursor = cursor;
        for (int tap = 0; tap < getNumTaps(); tap++) {
            float radians = tapPhase * M_PI;
            float coefficient = sinc(normalizedCutoff * radians) * hammingWindow(radians, spread);
            mCoefficients.at(cursor++) = coefficient;
            gain += coefficient;
            tapPhase += 1.0;
        }
        phase += phaseIncrement;
        while (phase >= 1.0) {
            phase -= 1.0;
        }

        // Correct for gain variations. // TODO review
        //printf("gain at %d was %f\n", i, gain);
        float gainCorrection = 1.0 / gain; // normalize the gain
        for (int tap = 0; tap < getNumTaps(); tap++) {
            float scaledCoefficient = mCoefficients.at(gainCursor) * gainCorrection;
            //printf("scaledCoefficient[%2d] = %10.6f\n", tap, scaledCoefficient);
            mCoefficients.at(gainCursor++) = scaledCoefficient;
        }
    }
}

void PolyphaseResampler::readFrame(float *frame) {
    // Clear accumulator for mixing.
    std::fill(mSingleFrame.begin(), mSingleFrame.end(), 0.0);

    // Multiply input times windowed sinc function.
    float *coefficients = &mCoefficients[mCoefficientCursor];
    float *xFrame = &mX[mCursor * getChannelCount()];
    for (int i = 0; i < mNumTaps; i++) {
        float coefficient = *coefficients++;
        for (int channel = 0; channel < getChannelCount(); channel++) {
            mSingleFrame[channel] += *xFrame++ * coefficient;
        }
    }

    // Advance and wrap through coefficients.
    mCoefficientCursor = (mCoefficientCursor + mNumTaps) % mCoefficients.size();

    // Copy accumulator to output.
    for (int channel = 0; channel < getChannelCount(); channel++) {
        frame[channel] = mSingleFrame[channel];
    }
}
