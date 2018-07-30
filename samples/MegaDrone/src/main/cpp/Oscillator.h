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


#ifndef MEGADRONE_OSCILLATOR_H
#define MEGADRONE_OSCILLATOR_H


#include <cstdint>
#include <atomic>
#include <math.h>
#include <memory>
#include "RenderableAudio.h"

constexpr double kDefaultFrequency = 440.0;
constexpr int32_t kDefaultSampleRate = 48000;
constexpr double kPi = M_PI;
constexpr double kTwoPi = kPi * 2;

template <typename T>
class Oscillator : public RenderableAudio<T> {

public:

    void setWaveOn(bool isWaveOn){
        mIsWaveOn.store(isWaveOn);
    };

    void setSampleRate(int32_t sampleRate){
        mSampleRate = sampleRate;
        updatePhaseIncrement();
    };

    void setFrequency(double frequency) {
        mFrequency = frequency;
        updatePhaseIncrement();
    };

    void setAmplitude(float amplitude);

    // From RenderableAudio<T>
    void renderAudio(T *audioData, int32_t numFrames) {

        if (mIsWaveOn){
            for (int i = 0; i < numFrames; ++i) {

                // Sine wave (sinf)
                //audioData[i*kChannelCount] = sinf(mPhase) * mAmplitude;

                // Square wave
                if (mPhase <= kPi){
                    audioData[i] = -mAmplitude;
                } else {
                    audioData[i] = mAmplitude;
                }

                mPhase += mPhaseIncrement;
                if (mPhase > kTwoPi) mPhase -= kTwoPi;
            }
        } else {
            memset(audioData, 0, sizeof(T) * numFrames);
        }
    };

private:
    std::atomic<bool> mIsWaveOn { false };
    float mPhase = 0.0;
    std::atomic<T> mAmplitude { 0 };
    std::atomic<double> mPhaseIncrement { 0.0 };
    double mFrequency = kDefaultFrequency;
    int32_t mSampleRate = kDefaultSampleRate;

    void updatePhaseIncrement(){
        mPhaseIncrement.store((kTwoPi * mFrequency) / static_cast<double>(mSampleRate));
    };
};

#endif //MEGADRONE_OSCILLATOR_H
