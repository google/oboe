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
#include "MonoToMultiConverter.h"

using namespace flowgraph;

MonoToMultiConverter::MonoToMultiConverter(int32_t channelCount)
        : input(*this, 1)
        , output(*this, channelCount) {
}

MonoToMultiConverter::~MonoToMultiConverter() { }

int32_t MonoToMultiConverter::onProcess(int64_t framePosition, int32_t numFrames) {
    int32_t framesToProcess = input.pullData(framePosition, numFrames);

    const float *inputBuffer = input.getBlock();
    float *outputBuffer = output.getBlock();
    int32_t channelCount = output.getSamplesPerFrame();
    // TODO maybe move to audio_util as audio_mono_to_multi()
    for (int i = 0; i < framesToProcess; i++) {
        // read one, write many
        float sample = *inputBuffer++;
        for (int channel = 0; channel < channelCount; channel++) {
            *outputBuffer++ = sample;
        }
    }
    return framesToProcess;
}

