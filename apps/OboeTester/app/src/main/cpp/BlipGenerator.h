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

#ifndef NATIVEOBOE_EXACTBLIPGENERATOR_H
#define NATIVEOBOE_EXACTBLIPGENERATOR_H

#include <atomic>
#include <math.h>
#include "flowgraph/FlowGraphNode.h"

class BlipGenerator : public oboe::flowgraph::FlowGraphNode {
public:
    BlipGenerator();
    virtual ~BlipGenerator() = default;

    void setSampleRate(int sampleRate);
    int32_t onProcess(int numFrames) override;
    void trigger();
    void reset() override;

    oboe::flowgraph::FlowGraphPortFloatOutput output;

private:
    static const int WAVETABLE_LENGTH = 2049;
    static const int NUM_WAVETABLE_SAMPLES = 2048; // LENGTH - 1

    static const int NUM_PULSE_FRAMES = (int) (48000 * (1.0 / 16.0));

    float mWaveTable[WAVETABLE_LENGTH];
    float mSrcPhase;
    float mPhaseIncr;
    int mNumPendingPulseFrames;

    std::atomic<int> mRequestCount;
    std::atomic<int> mAcknowledgeCount;
};

#endif // NATIVEOBOE_EXACTBLIPGENERATOR_H
