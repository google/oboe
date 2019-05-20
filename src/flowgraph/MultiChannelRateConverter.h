//
// Created by Phil Burk on 2019-05-17.
//

#ifndef OBOE_MULTICHANNEL_RATE_CONVERTER_H
#define OBOE_MULTICHANNEL_RATE_CONVERTER_H

#include <memory>
#include <sys/types.h>
#include <unistd.h>

namespace flowgraph {

class MultiChannelRateConverter {
public:
    explicit MultiChannelRateConverter(int32_t channelCount)
    : mChannelCount(channelCount) {
        mPreviousFrame = std::make_unique<float[]>(channelCount);
        mCurrentFrame = std::make_unique<float[]>(channelCount);
    }

    void writeFrame(const float *frame) {
        memcpy(mPreviousFrame.get(), mCurrentFrame.get(), sizeof(float) * mChannelCount);
        memcpy(mCurrentFrame.get(), frame, sizeof(float) * mChannelCount);
    }

    void readFrame(float *frame, double mPhase) {
        float *previous = mPreviousFrame.get();
        float *current = mCurrentFrame.get();
        for (int channel = 0; channel < mChannelCount; channel++) {
            float f0 = *previous++;
            float f1 = *current++;
            *frame++ = f0 + (mPhase * (f1 - f0));
        }
    }

private:
    const int mChannelCount;
    std::unique_ptr<float[]> mPreviousFrame;
    std::unique_ptr<float[]> mCurrentFrame;
};

}
#endif //OBOE_MULTICHANNEL_RATE_CONVERTER_H
