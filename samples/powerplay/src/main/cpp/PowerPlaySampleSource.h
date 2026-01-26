/*
 * Copyright 2026 The Android Open Source Project
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

#ifndef SAMPLES_POWERPLAYSAMPLESOURCE_H
#define SAMPLES_POWERPLAYSAMPLESOURCE_H

#include <player/OneShotSampleSource.h>

class PowerPlaySampleSource : public iolib::OneShotSampleSource {
public:
    using OneShotSampleSource::OneShotSampleSource;

    int32_t getPositionInFrames() const {
        if (mSampleBuffer == nullptr) return 0;
        int32_t channels = mSampleBuffer->getProperties().channelCount;
        if (channels <= 0) return 0;
        return mCurSampleIndex / channels;
    }

    void setPositionInFrames(int32_t frameIndex) {
        if (mSampleBuffer == nullptr) return;
        int32_t channels = mSampleBuffer->getProperties().channelCount;
        mCurSampleIndex = frameIndex * channels;
        if (mCurSampleIndex < 0) mCurSampleIndex = 0;
        if (mCurSampleIndex > mSampleBuffer->getNumSamples()) mCurSampleIndex = mSampleBuffer->getNumSamples();
    }
};

#endif //SAMPLES_POWERPLAYSAMPLESOURCE_H
