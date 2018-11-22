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

#include <math.h>
#include <unistd.h>
#include "AudioProcessorBase.h"

#include "ImpulseGenerator.h"

ImpulseGenerator::ImpulseGenerator()
        : OscillatorBase() {
}

AudioResult ImpulseGenerator::onProcess(
        uint64_t framePosition,
        int numFrames) {

    frequency.pullData(framePosition, numFrames);
    amplitude.pullData(framePosition, numFrames);

    const float *frequencies = frequency.getFloatBuffer(numFrames);
    const float *amplitudes = amplitude.getFloatBuffer(numFrames);
    float *buffer = output.getFloatBuffer(numFrames);

    for (int i = 0; i < numFrames; i++) {
        float value = 0.0f;
        mPhase += mFrequencyToPhase * frequencies[i];
        if (mPhase >= TWO_PI) {
            value = amplitudes[i];
            mPhase -= TWO_PI;
        }
        *buffer++ = value;
    }

    return AUDIO_RESULT_SUCCESS;
}
