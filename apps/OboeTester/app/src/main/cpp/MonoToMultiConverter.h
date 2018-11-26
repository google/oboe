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

#ifndef NATIVEOBOE_MONO_TO_MULTI_CONVERTER_H
#define NATIVEOBOE_MONO_TO_MULTI_CONVERTER_H

#include <unistd.h>
#include <sys/types.h>

#include "AudioProcessorBase.h"

class MonoToMultiConverter : AudioProcessorBase {
public:
    explicit MonoToMultiConverter(int32_t channelCount);

    virtual ~MonoToMultiConverter();

    AudioResult onProcess(
            uint64_t framePosition,
            int numFrames);

    void setEnabled(bool enabled) {};

    AudioInputPort input;
    AudioOutputPort output;

private:
};


#endif //NATIVEOBOE_MONO_TO_MULTI_CONVERTER_H
