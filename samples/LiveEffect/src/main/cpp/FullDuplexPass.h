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

#ifndef SAMPLES_FULLDUPLEXPASS_H
#define SAMPLES_FULLDUPLEXPASS_H
#include "FullDuplexStream.h"

class FullDuplexPass : public FullDuplexStream {
public:
    virtual oboe::DataCallbackResult
    onBothStreamsReady(const void *inputData, int numInputFrames, void *outputData,
                       int numOutputFrames) {
        size_t bytesPerFrame = this->getOutputStream()->getBytesPerFrame();
        size_t bytesToWrite = numInputFrames * bytesPerFrame;
        size_t byteDiff = (numOutputFrames - numInputFrames) * bytesPerFrame;
        size_t bytesToZero = (byteDiff > 0) ? byteDiff : 0;
        memcpy(outputData, inputData, bytesToWrite);
        memset((u_char*) outputData + bytesToWrite, 0, bytesToZero);
        return oboe::DataCallbackResult::Continue;
    }
};
#endif //SAMPLES_FULLDUPLEXPASS_H
