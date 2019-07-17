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

#ifndef FLOWGRAPH_POLYPHASE_RESAMPLER_H
#define FLOWGRAPH_POLYPHASE_RESAMPLER_H


#include <memory>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include "MultiChannelResampler.h"

namespace flowgraph {

class PolyphaseResampler : public MultiChannelResampler {
public:
    /**
     *
     * @param channelCount
     * @param numTaps
     * @param inputRate inputRate/outputRate should be a reduced fraction
     * @param outputRate
     */
    PolyphaseResampler(int32_t numTaps, int32_t inputRate, int32_t outputRate,
                       int32_t channelCount);

    virtual ~PolyphaseResampler() = default;

    void readFrame(float *frame) override;

    bool isWriteNeeded() const override {
        return mIntegerPhase >= mDenominator;
    }

    virtual void advanceWrite() override {
        mIntegerPhase -= mDenominator;
    }

    virtual void advanceRead() override {
        mIntegerPhase += mNumerator;
    }

protected:

    void generateCoefficients(int32_t inputRate, int32_t outputRate);

    std::vector<float>     mCoefficients;
    int32_t                mCoefficientCursor = 0;
    int32_t                mIntegerPhase = 0;
    int32_t                mNumerator = 0;
    int32_t                mDenominator = 0;

};

}

#endif //FLOWGRAPH_POLYPHASE_RESAMPLER_H
