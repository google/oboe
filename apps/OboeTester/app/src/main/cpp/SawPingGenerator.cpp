/*
 * Copyright 2015 The Android Open Source Project
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

#include <unistd.h>
#include "AudioProcessorBase.h"
#include "SawPingGenerator.h"
#include "oboe/Definitions.h"

SawPingGenerator::SawPingGenerator()
        : OscillatorBase()
        , mRequestCount(0)
        , mAcknowledgeCount(0)
        , mLevel(0.0f) {
}

SawPingGenerator::~SawPingGenerator() { }


AudioResult SawPingGenerator::onProcess(
        uint64_t framePosition,
        int numFrames) {

    frequency.pullData(framePosition, numFrames);
    amplitude.pullData(framePosition, numFrames);

    const float *frequencies = frequency.getFloatBuffer(numFrames);
    const float *amplitudes = amplitude.getFloatBuffer(numFrames);
    float *buffer = output.getFloatBuffer(numFrames);


    if (mRequestCount.load() > mAcknowledgeCount.load()) {
        mPhase = -1.0f;
        mLevel = 1.0;
        mAcknowledgeCount++;
    }

    // Check level to prevent numeric underflow.
    if (mLevel > 0.000001) {
        for (int i = 0; i < numFrames; i++) {
            float sawtooth = incrementPhase(frequencies[i]);
            *buffer++ = (float) (sawtooth * mLevel * amplitudes[i]);
            mLevel *= 0.999;
        }
    } else {
        for (int i = 0; i < numFrames; i++) {
            *buffer++ = 0.0f;
        }
    }

    return AUDIO_RESULT_SUCCESS;
}

void SawPingGenerator::setEnabled(bool enabled) {
    if (enabled) {
        mRequestCount++;
    }
}

