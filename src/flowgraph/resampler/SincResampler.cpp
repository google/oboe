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
        , mSingleFrame2(builder.getChannelCount()) {
    assert((getNumTaps() % 4) == 0); // Required for loop unrolling.
    generateCoefficients(builder.getInputRate(), builder.getOutputRate(), builder.getNormalizedCutoff());
}

// Generate coefficients from min to max.
void SincResampler::generateCoefficients(int32_t inputRate,
                                              int32_t outputRate,
                                              float normalizedCutoff) {
    mNumSeries = kMaxCoefficients / getNumTaps();
    mTablePhaseScaler = mNumSeries / 2.0f;
    mCoefficients.resize(getNumTaps() * mNumSeries);

    int cursor = 0;
    const float cutoffScaler = std::min(1.0f, normalizedCutoff * outputRate / inputRate);
    const int numTapsHalf = getNumTaps() / 2; // numTaps must be even.
    for (int i = 0; i < mNumSeries; i++) {
        float tapPhase = ((float)i / mNumSeries) - numTapsHalf;
        float gain = 0.0;
        int gainCursor = cursor;
        for (int tap = 0; tap < getNumTaps(); tap++) {
            float radians = tapPhase * M_PI;
            float coefficient = sinc(cutoffScaler * radians) * hammingWindow(radians, numTapsHalf);
            mCoefficients.at(cursor++) = coefficient;
            gain += coefficient;
            tapPhase += 1.0;
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

void SincResampler::readFrame(float *frame) {
    // Clear accumulator for mixing.
    std::fill(mSingleFrame.begin(), mSingleFrame.end(), 0.0);

    // Multiply input times windowed sinc function.
    float tablePhase = getPhase() * mTablePhaseScaler;
    long index1 = lrintf(tablePhase);
    float *coefficients1 = &mCoefficients[index1 * getNumTaps()];
    int index2 = (index1 + 1) * getNumTaps();
    if (index2 >= mCoefficients.size()) {
        index2 = 0;
    }
    float *coefficients2 = &mCoefficients[index2 * getNumTaps()];
    float *xFrame = &mX[mCursor * getChannelCount()];
    for (int i = 0; i < mNumTaps; i++) {
        float coefficient1 = *coefficients1++;
        float coefficient2 = *coefficients2++;
        for (int channel = 0; channel < getChannelCount(); channel++) {
            float sample = *xFrame++;
            mSingleFrame[channel] +=  sample* coefficient1;
            mSingleFrame2[channel] += sample * coefficient2;
        }
    }

    // Interpolate and copy to output.
    float fraction = tablePhase - index1;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        float low = mSingleFrame[channel];
        float high = mSingleFrame2[channel];
        frame[channel] = low + (fraction * (high - low));
    }
}

//void SincResampler::readFrame(float *frame) {
//    // Clear accumulator for mix.
//    for (int channel = 0; channel < getChannelCount(); channel++) {
//        mSingleFrame[channel] = 0.0;
//    }
//    float phase =  getPhase();
//    // Multiply input times windowed sinc function.
//    int xIndex = (mCursor + mNumTaps) * getChannelCount();
//    for (int i = 0; i < mNumTaps; i++) {
//        // TODO Use consecutive coefficient precomputed and then interpolate the result.
//        float coefficient = interpolateWindowedSinc(phase);
//        float *xFrame = &mX[xIndex];
//        for (int channel = 0; channel < getChannelCount(); channel++) {
//            mSingleFrame[channel] += coefficient * xFrame[channel];
//        }
//        xIndex -= getChannelCount();
//        phase += 1.0;
//    }
//    // Copy accumulator to output.
//    for (int channel = 0; channel < getChannelCount(); channel++) {
//        frame[channel] = mSingleFrame[channel];
//    }
//}

//float SincResampler::interpolateWindowedSinc(float phase) {
//    // convert from 0 to 2*kSpread range to 0 to table size range
//    float tablePhase = phase * mTablePhaseScaler;
//    //uint16_t tableIndex = fast_int_from_float(tablePhase);
//    long tableIndex = lrintf(tablePhase);
//    //int tableIndex = int(tablePhase);
//    float low = mWindowedSinc[tableIndex];
//    float high = mWindowedSinc[tableIndex + 1]; // OK because of guard point
//    float fraction = tablePhase - tableIndex;
//    return low + (fraction * (high - low));
//}

//void SincResampler::generateLookupTable() {
    // Place related coefficients together for faster convolution, like for Polyphase
    // but divide each zero crossing into 32 or 64 steps.
    // TODO store only half of function and use symmetry
    // By iterating over the table size we also set the guard point.
//    for (int i = 0; i < mWindowedSinc.size(); i++) {
//        float phase = (i * 2.0 * kSpread) / kNumPoints;
//        float radians = (phase - kSpread) * M_PI;
//        mWindowedSinc[i] = calculateWindowedSinc(radians, kSpread);
//    }
//}
