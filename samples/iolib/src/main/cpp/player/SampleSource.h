/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef _PLAYER_SAMPLESOURCE_
#define _PLAYER_SAMPLESOURCE_

#include <cstdint>

#include "DataSource.h"

#include "SampleBuffer.h"

namespace iolib {

/**
 * Defines an interface for audio data provided to a player object.
 * Concrete examples include OneShotSampleBuffer. One could imagine a LoopingSampleBuffer.
 */
class SampleSource: public DataSource {
public:
    SampleSource(SampleBuffer *sampleBuffer)
     : mSampleBuffer(sampleBuffer), mCurFrameIndex(0), mIsPlaying(false) {};
    virtual ~SampleSource() {};

    /*
     * Returns the audio properties of the audio data.
     */
    // AudioProperties getProperties() const { return mProperties; };

    void setPlayMode() { mCurFrameIndex = 0; mIsPlaying = true; }
    void setStopMode() { mIsPlaying = false; mCurFrameIndex = 0; }

    bool isPlaying() { return mIsPlaying; }

//    virtual void mixAudio(float* outBuff, int numFrames) =0;

protected:
    // AudioProperties mProperties;
    SampleBuffer    *mSampleBuffer;

    int32_t mCurFrameIndex;

    bool mIsPlaying;
};

} // namespace wavlib

#endif //_PLAYER_SAMPLESOURCE_
