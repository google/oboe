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

#ifndef MEGADRONE_AUDIOENGINE_H
#define MEGADRONE_AUDIOENGINE_H

#include <array>

#include <oboe/Oboe.h>
#include "Oscillator.h"
#include "Mixer.h"

constexpr int kNumOscillators = 100;

using namespace oboe;

class Synth : public AudioStreamCallback {
public:
    void start();
    void tap(bool isOn);

    DataCallbackResult
    onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    AudioStream *mStream;

    // Rendering objects for 16-bit integer output
    std::array<Oscillator<int16_t>, kNumOscillators> mOscsInt16;
    std::shared_ptr<Mixer<int16_t>> mMixerInt16;
    std::shared_ptr<RenderableAudio<int16_t>> mOutputStageInt16;

    // Rendering objects for floating point output
    std::array<Oscillator<float>, kNumOscillators> mOscsFloat;
    std::shared_ptr<Mixer<float>> mMixerFloat;
    std::shared_ptr<RenderableAudio<float>> mOutputStageFloat;



};


#endif //MEGADRONE_AUDIOENGINE_H
