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

#ifndef FLOWGRAPH_POLYPHASE_RESAMPLER_MONO_H
#define FLOWGRAPH_POLYPHASE_RESAMPLER_MONO_H

#include <sys/types.h>
#include <unistd.h>
#include "PolyphaseResampler.h"

namespace flowgraph {

    class PolyphaseResamplerMono : public PolyphaseResampler {
    public:
        PolyphaseResamplerMono(int32_t numTaps, int32_t inputRate, int32_t outputRate);

        virtual ~PolyphaseResamplerMono() = default;

        void writeFrame(const float *frame) override;

        void readFrame(float *frame) override;
    };

}

#endif //FLOWGRAPH_POLYPHASE_RESAMPLER_MONO_H
