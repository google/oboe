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

#ifndef OBOE_CONTINUOUS_RESAMPLER_H
#define OBOE_CONTINUOUS_RESAMPLER_H

#include <sys/types.h>
#include <unistd.h>
#include "MultiChannelResampler.h"

namespace resampler {

/*
 * Resampler that uses a double precision phase internally.
 */
class ContinuousResampler : public MultiChannelResampler {
public:
    explicit ContinuousResampler(const MultiChannelResampler::Builder &builder);

    virtual ~ContinuousResampler() = default;

    bool isWriteNeeded() const override {
        return mPhase >= 1.0;
    }

    virtual void advanceWrite() override {
        mPhase -= 1.0;
    }

    virtual void advanceRead() override {
        mPhase += mPhaseIncrement;
    }

    double getPhase() {
        return (float) mPhase;
    }

private:
    double mPhase = 1.0;
    double mPhaseIncrement = 1.0;
};

}
#endif //OBOE_CONTINUOUS_RESAMPLER_H
