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

#ifndef MEGADRONE_SYNTH_H
#define MEGADRONE_SYNTH_H

#include <array>

#include "shared/Oscillator.h"
#include "shared/Mixer.h"
#include "shared/MonoToStereo.h"

constexpr int kNumOscillators = 100;
constexpr float kOscBaseFrequency = 116.0;
constexpr float kOscDivisor = 33;
constexpr float kOscAmplitude = 0.009;


class Synth : public IRenderableAudio {
public:

    Synth(int32_t sampleRate, int32_t channelCount) {

        for (int i = 0; i < kNumOscillators; ++i) {
            mOscs[i].setSampleRate(sampleRate);
            mOscs[i].setFrequency(kOscBaseFrequency+(static_cast<float>(i)/kOscDivisor));
            mOscs[i].setAmplitude(kOscAmplitude);

            std::shared_ptr<IRenderableAudio> pOsc(&mOscs[i]);
            mMixer.addTrack(pOsc);
        }

        if (channelCount == oboe::ChannelCount::Stereo){
            mOutputStage = std::make_shared<MonoToStereo>(&mMixer);
        } else {
            mOutputStage.reset(&mMixer);
        }
    }

    // From ISynth
    void setWaveOn(bool isEnabled) {
        for (auto &osc : mOscs) osc.setWaveOn(isEnabled);
    };

    // From IRenderableAudio
    void renderAudio(float *audioData, int32_t numFrames) override {
        mOutputStage->renderAudio(audioData, numFrames);
    };

    virtual ~Synth() {}
private:

    // Rendering objects
    std::array<Oscillator, kNumOscillators> mOscs;
    Mixer mMixer;
    std::shared_ptr<IRenderableAudio> mOutputStage;
};


#endif //MEGADRONE_SYNTH_H
