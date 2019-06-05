//
// Created by atneya on 6/4/19.
//

#ifndef SAMPLES_RENDERABLETAP_H
#define SAMPLES_RENDERABLETAP_H

#include <stdint.h>

#include "IRenderableAudio.h"
/**
 * This class renders Float audio, but can be zeroed out.
 */
class RenderableTap : public IRenderableAudio {
public:
    RenderableTap(int32_t sampleRate, int32_t maxFrames, int32_t channelCount) :
    mSampleRate(sampleRate), mMaxFrames(maxFrames), mChannelCount(channelCount) { }
    virtual void setToneOn(bool isOn) = 0;
    const int32_t mSampleRate;
    const int32_t mMaxFrames;
    const int32_t mChannelCount;
};

#endif //SAMPLES_RENDERABLETAP_H