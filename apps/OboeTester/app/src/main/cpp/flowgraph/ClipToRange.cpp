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

#include <algorithm>
#include <unistd.h>
#include "AudioProcessorBase.h"
#include "ClipToRange.h"

using namespace flowgraph;

ClipToRange::ClipToRange(int32_t channelCount)
        : input(*this, channelCount)
        , output(*this, channelCount) {
}

int32_t ClipToRange::onProcess(int64_t framePosition, int32_t numFrames) {
    int32_t framesToProcess = input.pullData(framePosition, numFrames);
    const float *inputBuffer = input.getBlock();
    float *outputBuffer = output.getBlock();

    int32_t numSamples = framesToProcess * output.getSamplesPerFrame();
    for (int32_t i = 0; i < numSamples; i++) {
        *outputBuffer++ = std::min(mMaximum, std::max(mMinimum, *inputBuffer++));
    }

    return framesToProcess;
}
