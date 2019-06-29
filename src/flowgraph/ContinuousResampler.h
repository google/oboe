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

#ifndef FLOWGRAPH_CONTINUOUS_RESAMPLER_H
#define FLOWGRAPH_CONTINUOUS_RESAMPLER_H

#include <sys/types.h>
#include <unistd.h>
#include "MultiChannelResampler.h"

namespace flowgraph {

/*
 * Resampler that uses a double precision phase internally.
 */
class ContinuousResampler : public MultiChannelResampler {
public:
    explicit ContinuousResampler(int32_t channelCount,
                                 int32_t numTaps,
                                 int32_t inputRate,
                                 int32_t outputRate)
            : MultiChannelResampler(channelCount, numTaps, inputRate, outputRate) {
        mPhaseIncrement = (double) inputRate / (double) outputRate;
    }

    virtual ~ContinuousResampler() = default;

    bool isWriteReady() const override {
        return mPhase >= 1.0;
    }

    virtual void advanceWrite() override {
        mPhase -= 1.0;
    }

    bool isReadReady() const override {
        return mPhase < 1.0;
    }

    virtual void advanceRead() override {
        mPhase += mPhaseIncrement;
    }

    float getPhase() {
        return (float) mPhase;
    }

private:
    double mPhase = 1.0;
    double mPhaseIncrement = 1.0;
};

}
#endif //FLOWGRAPH_CONTINUOUS_RESAMPLER_H
