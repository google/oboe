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

#ifndef _PLAYER_ONESHOTSAMPLEBUFFER_
#define _PLAYER_ONESHOTSAMPLEBUFFER_

#include "SampleBuffer.h"

namespace wavlib {

class WavStreamReader;

/**
 * Provides audio data which will play through once when triggered
 */
class OneShotSampleBuffer: public SampleBuffer {
public:
    OneShotSampleBuffer() : SampleBuffer() {};
    virtual ~OneShotSampleBuffer() {};

    void loadSampleData(WavStreamReader* reader);
    void unloadSampleData();

    virtual void mixAudio(float* outBuff, int32_t numFrames);
};

} // namespace wavlib

#endif //_PLAYER_ONESHOTSAMPLEBUFFER_
