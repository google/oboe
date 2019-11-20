/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef PLAYER_DRUMSAMPLEDATASOURCE_H
#define PLAYER_DRUMSAMPLEDATASOURCE_H

#include "DataSource.h"

namespace wavlib {

class WavStreamReader;

class OneShotSampleBuffer: public DataSource {
public:
    OneShotSampleBuffer() : numSampleFrames(0), mCurFrameIndex(0), mIsPlaying(false) {};
    virtual ~OneShotSampleBuffer() {};

    virtual int64_t getSize() const;
    virtual AudioProperties getProperties() const { return mProperties; };
    virtual const float* getData() const;

    void loadSampleData(WavStreamReader* reader);

    void renderAudio(float* outBuff, int numFrames);
    void mixAudio(float* outBuff, int numFrames);

    void play() { mCurFrameIndex = 0; mIsPlaying = true; }
    bool isPlaying() { return mIsPlaying; }

private:
    AudioProperties mProperties;

    float*  mSampleData;
    int numSampleFrames;
    int mCurFrameIndex;

    bool mIsPlaying;

};

} // namespace wavlib

#endif //PLAYER_DRUMSAMPLEDATASOURCE_H
