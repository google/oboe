/*
 * Copyright 2026 The Android Open Source Project
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

#include "BlipGenerator.h"

BlipGenerator::BlipGenerator()
        : output(*this, 1)
        , mSrcPhase(0.0f)
        , mPhaseIncr(0.0f)
        , mNumPendingPulseFrames(0)
        , mRequestCount(0)
        , mAcknowledgeCount(0) {

    float incr = ((float)M_PI * 2.0f) / (float)NUM_WAVETABLE_SAMPLES;
    for(int i = 0; i < WAVETABLE_LENGTH; i++) {
        mWaveTable[i] = sinf(i * incr);
    }
}

void BlipGenerator::setSampleRate(int sampleRate) {
    float fn = sampleRate / (float)NUM_WAVETABLE_SAMPLES;
    float fnInverse = 1.0f / fn;
    mPhaseIncr = 1000.0f * fnInverse;
}

void BlipGenerator::reset() {
    FlowGraphNode::reset();
    mAcknowledgeCount.store(mRequestCount.load());
    mNumPendingPulseFrames = 0;
    mSrcPhase = 0.0f;
}

void BlipGenerator::trigger() {
    mRequestCount++;
}

int32_t BlipGenerator::onProcess(int numFrames) {
    float *buffer = output.getBuffer();

    if (mRequestCount.load() > mAcknowledgeCount.load()) {
        mAcknowledgeCount++;
        mNumPendingPulseFrames = NUM_PULSE_FRAMES;
    }

    if (mNumPendingPulseFrames <= 0) {
        for (int i = 0; i < numFrames; i++) {
            *buffer++ = 0.0f;
        }
    } else {
        for (int i = 0; i < numFrames; i++) {
            if (mNumPendingPulseFrames > 0) {
                while (mSrcPhase >= (float)NUM_WAVETABLE_SAMPLES) {
                    mSrcPhase -= (float)NUM_WAVETABLE_SAMPLES;
                }

                int srcIndex = (int)mSrcPhase;
                float delta = mSrcPhase - (float)srcIndex;
                float s0 = mWaveTable[srcIndex];
                float s1 = mWaveTable[srcIndex + 1];
                float value = s0 + ((s1 - s0) * delta);

                *buffer++ = value;
                mSrcPhase += mPhaseIncr;
                mNumPendingPulseFrames--;
            } else {
                *buffer++ = 0.0f;
            }
        }
    }

    return numFrames;
}