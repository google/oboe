/*
 * Copyright 2018 The Android Open Source Project
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


#ifndef SHARED_SYNTH_SOUND_H
#define SHARED_SYNTH_SOUND_H


#include <cstdint>
#include <atomic>
#include <math.h>
#include <memory>
#include "IRenderableAudio.h"

constexpr double kDefaultFrequency = 440.0;
constexpr int32_t kDefaultSampleRate = 48000;
constexpr double kPi = M_PI;
constexpr double kTwoPi = kPi * 2;

class SynthSound : public IRenderableAudio {

public:

    ~SynthSound() = default;

    void setWaveOn(bool isWaveOn) {
        mIsWaveOn.store(isWaveOn);
    };

    void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
        updatePhaseIncrement();
    };

    void setFrequency(double frequency) {
        mFrequency1 = frequency;
        mFrequency2 = frequency * 2;
        mFrequency3 = frequency * 3;
        mFrequency4 = frequency * 4;
        mFrequency5 = frequency * 5;

        updatePhaseIncrement();
    };

    // Amplitudes from https://epubs.siam.org/doi/pdf/10.1137/S00361445003822
    inline void setAmplitude(float amplitude) {
        mAmplitude1 = amplitude * .2f;
        mAmplitude2 = amplitude;
        mAmplitude3 = amplitude * .1f;
        mAmplitude4 = amplitude * .02f;
        mAmplitude5 = amplitude * .15f;
    };

    // From IRenderableAudio
    void renderAudio(float *audioData, int32_t numFrames) override {

        if (mIsWaveOn){
            for (int i = 0; i < numFrames; ++i) {
                audioData[i] = sinf(mPhase1) * mAmplitude1;
                audioData[i] += sinf(mPhase2) * mAmplitude2;
                audioData[i] += sinf(mPhase3) * mAmplitude3;
                audioData[i] += sinf(mPhase4) * mAmplitude4;
                audioData[i] += sinf(mPhase5) * mAmplitude5;

                mPhase1 += mPhaseIncrement1;
                if (mPhase1 > kTwoPi) mPhase1 -= kTwoPi;
                mPhase2 += mPhaseIncrement2;
                if (mPhase2 > kTwoPi) mPhase2 -= kTwoPi;
                mPhase3 += mPhaseIncrement3;
                if (mPhase3 > kTwoPi) mPhase3 -= kTwoPi;
                mPhase4 += mPhaseIncrement4;
                if (mPhase4 > kTwoPi) mPhase4 -= kTwoPi;
                mPhase5 += mPhaseIncrement5;
                if (mPhase5 > kTwoPi) mPhase5 -= kTwoPi;
            }
        } else {
            memset(audioData, 0, sizeof(float) * numFrames);
        }
    };

private:
    std::atomic<bool> mIsWaveOn { false };
    float mPhase1 = 0.0;
    float mPhase2 = 0.0;
    float mPhase3 = 0.0;
    float mPhase4 = 0.0;
    float mPhase5 = 0.0;
    std::atomic<float> mAmplitude1 { 0 };
    std::atomic<float> mAmplitude2 { 0 };
    std::atomic<float> mAmplitude3 { 0 };
    std::atomic<float> mAmplitude4 { 0 };
    std::atomic<float> mAmplitude5 { 0 };
    std::atomic<double> mPhaseIncrement1 { 0.0 };
    std::atomic<double> mPhaseIncrement2 { 0.0 };
    std::atomic<double> mPhaseIncrement3 { 0.0 };
    std::atomic<double> mPhaseIncrement4 { 0.0 };
    std::atomic<double> mPhaseIncrement5 { 0.0 };
    double mFrequency1 = kDefaultFrequency;
    double mFrequency2 = kDefaultFrequency * 2;
    double mFrequency3 = kDefaultFrequency * 3;
    double mFrequency4 = kDefaultFrequency * 4;
    double mFrequency5 = kDefaultFrequency * 5;
    int32_t mSampleRate = kDefaultSampleRate;

    void updatePhaseIncrement(){
        mPhaseIncrement1.store((kTwoPi * mFrequency1) / static_cast<double>(mSampleRate));
        mPhaseIncrement2.store((kTwoPi * mFrequency2) / static_cast<double>(mSampleRate));
        mPhaseIncrement3.store((kTwoPi * mFrequency3) / static_cast<double>(mSampleRate));
        mPhaseIncrement4.store((kTwoPi * mFrequency4) / static_cast<double>(mSampleRate));
        mPhaseIncrement5.store((kTwoPi * mFrequency5) / static_cast<double>(mSampleRate));
    };
};

#endif //SHARED_SYNTH_SOUND_H
