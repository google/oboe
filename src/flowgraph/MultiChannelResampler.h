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

#ifndef FLOWGRAPH_MULTICHANNEL_RESAMPLER_H
#define FLOWGRAPH_MULTICHANNEL_RESAMPLER_H

#include <memory>
#include <vector>
#include <sys/types.h>
#include <unistd.h>

namespace flowgraph {

class MultiChannelResampler {

public:

    MultiChannelResampler(int32_t channelCount,
                          int32_t numTaps,
                          int32_t inputRate,
                          int32_t outputRate);

    virtual ~MultiChannelResampler() = default;

    virtual bool isWriteNeeded() const = 0;

    virtual void advanceWrite() = 0;
    virtual void advanceRead() = 0;

    /**
     * Write a frame containing N samples.
     * @param frame pointer to the first sample in a frame
     */
    virtual void writeFrame(const float *frame);

    /**
     * Read a frame containing N samples using interpolation.
     * @param frame pointer to the first sample in a frame
     * @param phase phase between 0.0 and 1.0 for interpolation
     */
    virtual void readFrame(float *frame) = 0;

    int getNumTaps() const {
        return mNumTaps;
    }

    int getChannelCount() const {
        return mChannelCount;
    }

    /**
     * @param phase between 0.0 and  2*spread
     * @return windowedSinc
     */
    static float calculateWindowedSinc(float phase, int spread);

    enum class Quality : int32_t {
        Low,
        Medium,
        High,
        Best,
    };

    static MultiChannelResampler *make(int32_t channelCount,
            int32_t inputRate,
            int32_t outputRate,
            Quality quality);

private:

    const int          mChannelCount;

protected:

    const int          mNumTaps;
    int                mCursor = 0;
    std::vector<float> mX;
    std::vector<float> mSingleFrame;


};

}
#endif //FLOWGRAPH_MULTICHANNEL_RESAMPLER_H
