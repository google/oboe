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
constexpr int32_t kNumSineWaves = 5;
constexpr double kSustainMultiplier = 0.99999;
constexpr double kReleaseMultiplier = 0.999;
// Stop playing music below this cutoff
constexpr double kMasterAmplitudeCutOff = 0.01;

class SynthSound : public IRenderableAudio {

public:
    SynthSound() {
    }
    ~SynthSound() = default;

    void setWaveOn(bool isWaveOn) {
        if (!mIsWaveOn && isWaveOn) {
            mMasterAmplitude = 1.0f;
        }
        mIsWaveOn.store(isWaveOn);
    };
    void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
        updatePhaseIncrement();
    };
    void setFrequency(double frequency) {
        mFrequency = frequency;
        updatePhaseIncrement();
    };
    // Amplitudes from https://epubs.siam.org/doi/pdf/10.1137/S00361445003822
    inline void setAmplitude(float amplitude) {
        mAmplitudes[0] = amplitude * .2f;
        mAmplitudes[1] = amplitude;
        mAmplitudes[2] = amplitude * .1f;
        mAmplitudes[3] = amplitude * .02f;
        mAmplitudes[4] = amplitude * .15f;
    };
    // From IRenderableAudio
    void renderAudio(float *audioData, int32_t numFrames) override {
        if (mIsWaveOn){
            for (int i = 0; (i < numFrames) && (mMasterAmplitude > kMasterAmplitudeCutOff); ++i) {
                mMasterAmplitude.store(mMasterAmplitude * kSustainMultiplier);
                audioData[i] = 0;
                for (int j = 0; j < kNumSineWaves; ++j) {
                    audioData[i] += sinf(mPhase * (j + 1)) * mAmplitudes[j] * mMasterAmplitude;
                }
                mPhase.store(mPhase + mPhaseIncrement);
                if (mPhase > kTwoPi) {
                    mPhase.store(mPhase - kTwoPi);
                }
            }
        } else {
            memset(audioData, 0, sizeof(float) * numFrames);
            // Remove some noise by letting the current sine wave complete.
            // Since all the frequencies are multiples of mPhases[0], if the first wave complete,
            // so should the others. Use an extra release period to reduce even more noise.
            for (int i = 0; (i < numFrames) && (mMasterAmplitude > kMasterAmplitudeCutOff); ++i) {
                mMasterAmplitude.store(mMasterAmplitude * kReleaseMultiplier);
                for (int j = 0; j < kNumSineWaves; ++j) {
                    audioData[i] += sinf(mPhase * (j + 1)) * mAmplitudes[j] * mMasterAmplitude;
                }
                mPhase.store(mPhase + mPhaseIncrement);
            }
        }
    };

private:
    std::atomic<double> mMasterAmplitude;
    std::atomic<bool> mIsWaveOn { false };
    std::array<std::atomic<float>, kNumSineWaves> mAmplitudes;
    std::atomic<float> mPhase;
    std::atomic<double> mPhaseIncrement;
    std::atomic<double> mFrequency { kDefaultFrequency };
    std::atomic<int32_t> mSampleRate { kDefaultSampleRate };
    void updatePhaseIncrement(){
        mPhaseIncrement = kTwoPi * mFrequency / static_cast<double>(mSampleRate);
    };
};
#endif //SHARED_SYNTH_SOUND_H