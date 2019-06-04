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

#ifndef OBOE_MULTICHANNEL_RATE_CONVERTER_H
#define OBOE_MULTICHANNEL_RATE_CONVERTER_H

#include <memory>
#include <sys/types.h>
#include <unistd.h>

namespace flowgraph {

class MultiChannelResampler {
public:
    explicit MultiChannelResampler(int32_t channelCount)
        : mChannelCount(channelCount) {}

    virtual ~MultiChannelResampler() = default;

    /**
     * Write a frame containing N samples.
     * @param frame
     */
    virtual void writeFrame(const float *frame) = 0;

    /**
     * Read a frame containing N samples using interpolation.
     * @param frame
     * @param phase phase between 0.0 and 1.0 for interpolation
     */
    virtual void readFrame(float *frame, float phase) = 0;

    int getChannelCount() const {
        return mChannelCount;
    }

private:
    const int mChannelCount;
};

}
#endif //OBOE_MULTICHANNEL_RATE_CONVERTER_H
