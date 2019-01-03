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

class OscillatorBase : public flowgraph::AudioProcessorBase {
public:
    OscillatorBase();

    virtual ~OscillatorBase() = default;

    void setSampleRate(float sampleRate) {
        mSampleRate = sampleRate;
        mFrequencyToPhaseIncrement = 1.0f / sampleRate; // scaler
    }

    float setSampleRate() {
        return mSampleRate;
    }

    /**
     * Control the frequency of the oscillator in Hz.
     */
    flowgraph::AudioFloatInputPort  frequency;

    /**
     * Control the linear amplitude of the oscillator.
     * Silence is 0.0.
     * A typical full amplitude would be 1.0.
     */
    flowgraph::AudioFloatInputPort  amplitude;

    flowgraph::AudioFloatOutputPort output;

protected:
    float incrementPhase(float frequency) {
        mPhase += frequency * mFrequencyToPhaseIncrement;
        // Wrap phase in the range of -1 to +1
        if (mPhase >= 1.0f) {
            mPhase -= 2.0f;
        } else if (mPhase < -1.0f) {
            mPhase += 2.0f;
        }
        return mPhase;
    }

    float   mPhase = 0.0;  // phase that ranges from -1.0 to +1.0
    float   mSampleRate = 0.0f;
    float   mFrequencyToPhaseIncrement = 0.0f; // Scaler for converting frequency to phase increment.
};


#endif //NATIVEOBOE_OSCILLATORBASE_H
