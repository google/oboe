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

#include <unistd.h>
#include "AudioProcessorBase.h"
#include "ManyToMultiConverter.h"

ManyToMultiConverter::ManyToMultiConverter(int32_t channelCount)
        : inputs(channelCount)
        , output(*this, channelCount) {
    for (int i = 0; i < channelCount; i++) {
        inputs[i] = std::make_unique<AudioInputPort>(*this, 1);
    }
}

AudioResult ManyToMultiConverter::onProcess(
        uint64_t framePosition,
        int numFrames) {
    int32_t channelCount = output.getSamplesPerFrame();

    for (int i = 0; i < channelCount; i++) {
        inputs[i]->pullData(framePosition, numFrames);
    }


    for (int ch = 0; ch < channelCount; ch++) {
        const float *inputBuffer = inputs[ch]->getFloatBuffer(numFrames);
        float *outputBuffer = output.getFloatBuffer(numFrames) + ch;

        for (int i = 0; i < numFrames; i++) {
            // read one, write into the proper interleaved output channel
            float sample = *inputBuffer++;
            *outputBuffer = sample;
            outputBuffer += channelCount; // advance to next multichannel frame
        }
    }
    return AUDIO_RESULT_SUCCESS;
}

