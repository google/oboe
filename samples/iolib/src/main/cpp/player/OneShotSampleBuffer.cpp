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

#include <string.h>

#include "io/wav/WavStreamReader.h"

#include "OneShotSampleBuffer.h"

namespace wavlib {

void OneShotSampleBuffer::loadSampleData(WavStreamReader* reader) {
    mProperties.channelCount = reader->getNumChannels();
    mProperties.sampleRate = reader->getSampleRate();

    reader->positionToAudio();

    numSampleFrames = reader->getNumSampleFrames() * reader->getNumChannels();
    mSampleData = new float[numSampleFrames];
    reader->getDataFloat(mSampleData, reader->getNumSampleFrames());
}

void OneShotSampleBuffer::unloadSampleData() {
    delete[] mSampleData;
    mSampleData = nullptr;
    numSampleFrames = 0;

    // kinda by definition..
    mCurFrameIndex = 0;
    mIsPlaying = false;
}

void OneShotSampleBuffer::mixAudio(float* outBuff, int32_t numFrames) {
    int32_t numWriteFrames = mIsPlaying
                         ? std::min(numFrames, numSampleFrames - mCurFrameIndex)
                         : 0;

    if (numWriteFrames != 0) {
        // Mix in the samples
        int32_t lastIndex = mCurFrameIndex + numWriteFrames;
        for(int32_t index = 0; index < numWriteFrames; index++) {
            outBuff[index] += mSampleData[mCurFrameIndex++];
        }

        if (mCurFrameIndex >= numSampleFrames) {
            mIsPlaying = false;
        }
    }

    // silence
    // no need as the output buffer would need to have been filled with silence
    // to be mixed into
}

} // namespace wavlib
