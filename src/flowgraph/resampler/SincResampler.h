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

    int getSpread() const {
        return kSpread;
    }

    /**
     * @param phase between 0.0 and  2*kSpread
     * @return windowedSinc
     */
    float interpolateWindowedSinc(float phase);

protected:

    // Number of zero crossings on one side of central lobe.
    // Higher numbers provide higher quality but use more CPU.
    // 2 is the minimum one should use.
    static constexpr int   kSpread = 10;
    static constexpr int   kNumTaps = kSpread * 2; // TODO should be odd, not even

    std::vector<float>     mWindowedSinc;

private:

    void generateLookupTable();

    // Size of the lookup table.
    // Higher numbers provide higher accuracy and quality but use more memory.
    static constexpr int   kNumPoints = 4096;
    static constexpr int   kNumGuardPoints = 1;

    static constexpr float kTablePhaseScaler = kNumPoints / (2.0 * kSpread);

};

}
#endif //OBOE_SINC_RESAMPLER_H
