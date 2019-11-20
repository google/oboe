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

#include "io/wav/WavStreamReader.h"

#include "OneShotSampleBuffer.h"

namespace wavlib {

int64_t OneShotSampleBuffer::getSize() const {
    return 0;
}

const float* OneShotSampleBuffer::getData() const {
    return mSampleData;
}

void OneShotSampleBuffer::loadSampleData(WavStreamReader* reader) {
    mProperties.channelCount = reader->getNumChannels();
    mProperties.sampleRate = reader->getSampleRate();

    reader->positionToAudio();

    // For now we know that the source sample format is 16bit
    numSampleFrames = reader->getNumSampleFrames() * reader->getNumChannels();
    mSampleData = new float[numSampleFrames];
    reader->getDataFloat(mSampleData, reader->getNumSampleFrames());
}

void OneShotSampleBuffer::renderAudio(float* outBuff, int numFrames) {
    int numWriteFrames = mIsPlaying
            ? std::min(numFrames, numSampleFrames - mCurFrameIndex)
            : 0;

    if (numWriteFrames != 0) {
        // Sample Audio
        memcpy(outBuff, mSampleData + mCurFrameIndex, numWriteFrames * sizeof(float));
        // advance
        mCurFrameIndex += numWriteFrames;
        if (mCurFrameIndex >= numSampleFrames) {
            mIsPlaying = false;
        }
    }

    // silence
    int remainingFrames = numWriteFrames - numWriteFrames;
    if (remainingFrames != 0) {
        memset(outBuff + numWriteFrames, 0, remainingFrames);
    }
}

void OneShotSampleBuffer::mixAudio(float* outBuff, int numFrames) {
    int numWriteFrames = mIsPlaying
                         ? std::min(numFrames, numSampleFrames - mCurFrameIndex)
                         : 0;

    if (numWriteFrames != 0) {
        // Sample Audio
        for(int index = 0; index < numWriteFrames; index++) {
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
