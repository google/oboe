/*
 * Copyright 2019 The Android Open Source Project
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

#include "common/OboeDebug.h"
#include "FullDuplexEcho.h"

oboe::Result  FullDuplexEcho::start() {
    int32_t delayFrames = (int32_t) (kMaxDelayTimeSeconds * getOutputStream()->getSampleRate());
    mDelayLine = std::make_unique<InterpolatingDelayLine>(delayFrames);
    return FullDuplexStream::start();
}

oboe::DataCallbackResult FullDuplexEcho::onBothStreamsReady(
        const void *inputData,
        int   numInputFrames,
        void *outputData,
        int   numOutputFrames) {
    // FIXME only handles matching stream formats.
    // TODO Add delay node
    // TODO use flowgraph to handle format conversion
    int32_t framesToEcho = std::min(numInputFrames, numOutputFrames);
    float *inputFloat = (float *)inputData;
    float *outputFloat = (float *)outputData;
    // zero out entire output array
    memset(outputFloat, 0, numOutputFrames * getOutputStream()->getBytesPerFrame());

    int32_t inputStride = getInputStream()->getChannelCount();
    int32_t outputStride = getOutputStream()->getChannelCount();
    float delayFrames = mDelayTimeSeconds * getOutputStream()->getSampleRate();
    while (framesToEcho-- > 0) {
        *outputFloat = mDelayLine->process(delayFrames, *inputFloat); // mono delay
        inputFloat += inputStride;
        outputFloat += outputStride;
    }
    return oboe::DataCallbackResult::Continue;
};
