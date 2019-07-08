/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef SAMPLES_RENDERABLE_TAP_H
#define SAMPLES_RENDERABLE_TAP_H

#include <stdint.h>

#include "IRenderableAudio.h"
/**
 * This class renders Float audio, but can be zeroed out.
 */
class RenderableTap : public IRenderableAudio {
public:
    RenderableTap(int32_t sampleRate, int32_t channelCount) :
    mSampleRate(sampleRate), mChannelCount(channelCount) { }
    virtual void setToneOn(bool isOn) = 0;
    const int32_t mSampleRate;
    const int32_t mChannelCount;
};

#endif //SAMPLES_RENDERABLE_TAP_H