/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef _PLAYER_SAMPLEBUFFER_
#define _PLAYER_SAMPLEBUFFER_

#include <io/wav/WavStreamReader.h>

using parselib::WavStreamReader;

namespace iolib {

/*
 * Defines the relevant properties of the audio data being sourced.
 */
struct AudioProperties {
    int32_t channelCount;
    int32_t sampleRate;
};

class SampleBuffer {
public:
    SampleBuffer() : mSampleData(nullptr), mNumSampleFrames(0) {};

    // Data load/unload
    void loadSampleData(WavStreamReader* reader);
    void unloadSampleData();

    virtual AudioProperties getProperties() const { return mAudioProperties; }

    const float* getSampleData() { return mSampleData; }
    int32_t getNumSampleFrames() { return mNumSampleFrames; }

protected:
    AudioProperties mAudioProperties;

    float*  mSampleData;
    int32_t mNumSampleFrames;
};

}

#endif //_PLAYER_SAMPLEBUFFER_
