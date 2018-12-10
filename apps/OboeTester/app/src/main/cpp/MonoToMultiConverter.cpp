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

MonoToMultiConverter::MonoToMultiConverter(int32_t channelCount)
        : input(*this, 1)
        , output(*this, channelCount) {
}

AudioResult MonoToMultiConverter::onProcess(
        uint64_t framePosition,
        int numFrames) {
    input.pullData(framePosition, numFrames);

    const float *inputBuffer = input.getFloatBuffer(numFrames);
    float *outputBuffer = output.getFloatBuffer(numFrames);
    int32_t channelCount = output.getSamplesPerFrame();

    for (int i = 0; i < numFrames; i++) {
        // read one, write many
        float sample = *inputBuffer++;
        for (int ch = 0; ch < channelCount; ch++) {
            *outputBuffer++ = sample;
        }
    }
    return AUDIO_RESULT_SUCCESS;
}

