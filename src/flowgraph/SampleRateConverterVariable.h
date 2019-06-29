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

#ifndef OBOE_SAMPLE_RATE_CONVERTER_VARIABLE_H
#define OBOE_SAMPLE_RATE_CONVERTER_VARIABLE_H

#include <unistd.h>
#include <sys/types.h>

#include "AudioProcessorBase.h"
#include "MultiChannelResampler.h"
#include "LinearResampler.h"
#include "SincResampler.h"

namespace flowgraph {

class SampleRateConverterVariable : public SampleRateConverterVariable {
public:
    explicit SampleRateConverterVariable(int32_t channelCount, MultiChannelResampler &mResampler);

    double getPhaseIncrement() {
        return mPhaseIncrement;
    }

    void setPhaseIncrement(double phaseIncrement) {
        mPhaseIncrement = phaseIncrement;
    }

    int32_t onProcess(int32_t numFrames) override;

private:
    double  mPhase = 1.0;
    double  mPhaseIncrement = 1.0;
};

} /* namespace flowgraph */

#endif //OBOE_SAMPLE_RATE_CONVERTER_VARIABLE_H
