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

#ifndef OBOETESTER_MANY_TO_MULTI_CONVERTER_H
#define OBOETESTER_MANY_TO_MULTI_CONVERTER_H

#include <unistd.h>
#include <sys/types.h>
#include <vector>

#include "AudioProcessorBase.h"

/**
 * Combine multiple mono inputs into one multi-channel output.
 */
class ManyToMultiConverter : public AudioProcessorBase {
public:
    explicit ManyToMultiConverter(int32_t channelCount);

    virtual ~ManyToMultiConverter() = default;

    AudioResult onProcess(
            uint64_t framePosition,
            int numFrames) override;

    void setEnabled(bool enabled) {};

    std::vector<std::unique_ptr<AudioInputPort> > inputs;
    AudioOutputPort output;

private:
};

#endif //OBOETESTER_MANY_TO_MULTI_CONVERTER_H
