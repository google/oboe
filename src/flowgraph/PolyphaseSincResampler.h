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

#ifndef FLOWGRAPH_POLYPHASE_SINC_RESAMPLER_H
#define FLOWGRAPH_POLYPHASE_SINC_RESAMPLER_H


#include <memory>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include "MultiChannelResampler.h"

namespace flowgraph {

class PolyphaseSincResampler : public MultiChannelResampler {
public:
    PolyphaseSincResampler(int32_t channelCount, int32_t inputRate, int32_t outputRate);

    virtual ~PolyphaseSincResampler() = default;

    void readFrame(float *frame) override;

    int getSpread() const {
        return kSpread;
    }

    bool isWriteReady() const override {
        return mIntegerPhase >= mDenominator;
    }

    virtual void advanceWrite() override {
        mIntegerPhase -= mDenominator;
    }

    bool isReadReady() const override {
        return mIntegerPhase < mDenominator;
    }

    virtual void advanceRead() override {
        mIntegerPhase += mNumerator;
    }

protected:

    void generateCoefficients(int32_t inputRate, int32_t outputRate);
    
    // Number of zero crossings on one side of central lobe.
    // Higher numbers provide higher quality but use more CPU.
    // 2 is the minimum one should use.
    static constexpr int   kSpread = 10;
    static constexpr int   kNumTaps = kSpread * 2; // TODO should be odd, not even

    std::vector<float>     mCoefficients;
    int32_t                mCoefficientCursor = 0;
    int32_t                mIntegerPhase = 0;
    int32_t                mNumerator = 0;
    int32_t                mDenominator = 0;

};

}

#endif //FLOWGRAPH_POLYPHASE_SINC_RESAMPLER_H
