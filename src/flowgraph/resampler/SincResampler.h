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

#ifndef OBOE_SINC_RESAMPLER_H
#define OBOE_SINC_RESAMPLER_H

#include <memory>
#include <sys/types.h>
#include <unistd.h>
#include "ContinuousResampler.h"

namespace resampler {

class SincResampler : public ContinuousResampler {
public:
    explicit SincResampler(const MultiChannelResampler::Builder &builder);

    virtual ~SincResampler() = default;

    void readFrame(float *frame) override;

    /**
     * @param phase between 0.0 and  2*kSpread
     * @return windowedSinc
     */
    float interpolateWindowedSinc(float phase);

protected:

    std::vector<float>     mWindowedSinc;

private:

    /**
     * Generate the filter coefficients in optimal order.
     * @param inputRate
     * @param outputRate
     * @param normalizedCutoff filter cutoff frequency normalized to Nyquist rate of output
     */
    void generateCoefficients(int32_t inputRate,
                              int32_t outputRate,
                              float normalizedCutoff);

    std::vector<float>     mCoefficients;
    int32_t                mNumSeries = 0;
    std::vector<float>     mSingleFrame2; // for interpolation


    float mTablePhaseScaler = 0.0f;

};

}
#endif //OBOE_SINC_RESAMPLER_H
