/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OBOE_HELLOOBOE_SINE_GENERATOR_H
#define OBOE_HELLOOBOE_SINE_GENERATOR_H

#include <math.h>
#include <cstdint>

class SineGenerator
{
public:
    SineGenerator();
    ~SineGenerator() = default;

    void setup(double frequency, int32_t frameRate);

    void setup(double frequency, int32_t frameRate, float amplitude);

    void setSweep(double frequencyLow, double frequencyHigh, double seconds);

    void render(int16_t *buffer, int32_t channelStride, int32_t numFrames);

    void render(float *buffer, int32_t channelStride, int32_t numFrames);

private:
    double mAmplitude;
    double mPhase = 0.0;
    int32_t mFrameRate;
    double mPhaseIncrement;
    double mPhaseIncrementLow;
    double mPhaseIncrementHigh;
    double mUpScaler = 1.0;
    double mDownScaler = 1.0;
    bool   mGoingUp = false;
    bool   mSweeping = false;

    void advancePhase();

    double getPhaseIncremement(double frequency);
};

#endif /* OBOE_HELLOOBOE_SINE_GENERATOR_H */
