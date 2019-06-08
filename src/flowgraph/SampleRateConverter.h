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

#ifndef OBOE_SAMPLE_RATE_CONVERTER_H
#define OBOE_SAMPLE_RATE_CONVERTER_H

#include <unistd.h>
#include <sys/types.h>

#include "AudioProcessorBase.h"
#include "MultiChannelResampler.h"
#include "LinearResampler.h"
#include "SincResampler.h"

namespace flowgraph {

class SampleRateConverter : public AudioFilter {
public:
    explicit SampleRateConverter(int32_t channelCount, MultiChannelResampler &mResampler);

    virtual ~SampleRateConverter() = default;

    int32_t onProcess(int32_t numFrames) override;

    double getPhaseIncrement() {
        return mPhaseIncrement;
    }

    void setPhaseIncrement(double phaseIncrement) {
        mPhaseIncrement = phaseIncrement;
    }

    const char *getName() override {
        return "SampleRateConverter";
    }

private:

    // Return true if there is a sample available.
    bool isInputAvailable();

    const float *getNextInputFrame();

    MultiChannelResampler &mResampler;

    double  mPhase = 1.0;
    double  mPhaseIncrement = 1.0;
    int32_t mInputCursor = 0;
    int32_t mInputValid = 0;
    int64_t mInputFramePosition = 0; // monotonic counter of input frames used for pullData

};
} /* namespace flowgraph */
#endif //OBOE_SAMPLE_RATE_CONVERTER_H
