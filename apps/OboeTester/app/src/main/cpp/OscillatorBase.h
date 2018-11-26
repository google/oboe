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

#ifndef NATIVEOBOE_OSCILLATORBASE_H
#define NATIVEOBOE_OSCILLATORBASE_H

#include "AudioProcessorBase.h"

constexpr float TWO_PI = (float)(2.0 * M_PI);

class OscillatorBase : public AudioProcessorBase {
public:
    OscillatorBase();

    virtual ~OscillatorBase() = default;

    void setSampleRate(float sampleRate) {
        mSampleRate = sampleRate;
        mFrequencyToPhase = TWO_PI / sampleRate; // scaler
    }

    float setSampleRate() {
        return mSampleRate;
    }

    AudioInputPort  frequency;
    AudioInputPort  amplitude;
    AudioOutputPort output;

protected:
    float incrementPhase(float frequency) {
        mPhase += frequency * mFrequencyToPhase;
        if (mPhase >= TWO_PI) {
            mPhase -= TWO_PI;
        } else if (mPhase < 0) {
            mPhase += TWO_PI;
        }
        return mPhase;
    }

    float   mPhase = 0.0;
    float   mSampleRate;
    float   mFrequencyToPhase;
};


#endif //NATIVEOBOE_OSCILLATORBASE_H
