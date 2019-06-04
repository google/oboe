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
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include "MultiChannelResampler.h"

namespace flowgraph {

class SincResampler : public MultiChannelResampler{
public:
    explicit SincResampler(int32_t channelCount);

    void writeFrame(const float *frame) override;

    void readFrame(float *frame, float mPhase) override;

    int getSpread() const {
        return kSpread;
    }

    /**
     * @param phase between 0.0 and  2*kSpread
     * @return windowedSinc
     */
    float interpolateWindowedSinc(float phase);

    /**
     * @param phase between 0.0 and  2*kSpread
     * @return windowedSinc
     */
    float calculateWindowedSinc(float phase);

private:
    void generateLookupTable();

    static constexpr int kSpread = 3; // number of zero crossing on one side of central lobe
    static constexpr int kNumTaps = kSpread * 2;
    static constexpr int kNumGuardPoints = 1;
    static constexpr int kNumPoints = 1024;

    std::vector<float> mX;
    std::vector<float> mSingleFrame;
    std::vector<float> mWindowedSinc;
    int mCursor = 0;
};

}
#endif //OBOE_SINC_RESAMPLER_H
