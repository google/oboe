/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef NATIVEOBOE_MULTICHANNEL_RECORDING_H
#define NATIVEOBOE_MULTICHANNEL_RECORDING_H

#include <memory.h>
#include <unistd.h>
#include <sys/types.h>

class MultiChannelRecording {
public:
    MultiChannelRecording(int32_t channelCount, int32_t maxFrames)
            : mChannelCount(channelCount)
            , mMaxFrames(maxFrames) {
        mData = new float[channelCount * maxFrames];
    }

    ~MultiChannelRecording() {
        delete[] mData;
    }

    void rewind() {
        mCursor = 0;
    }

    /**
     * Write numFrames from the short buffer into the recording, if there is room.
     * Convert shoirts to floats.
     *
     * @param buffer
     * @param numFrames
     * @return number of frames actually written.
     */
    int32_t write(int16_t *buffer, int32_t numFrames) {
        int32_t framesEmpty = mMaxFrames - mValidFrames;
        if (numFrames > framesEmpty) {
            numFrames = framesEmpty;
        }
        if (numFrames > 0) {
            int32_t numSamples = numFrames * mChannelCount;
            int32_t index = mValidFrames * mChannelCount;
            for (int i = 0; i < numSamples; i++) {
                mData[index++] = buffer[i * mChannelCount] * (1.0f / 32768);
            }
            mValidFrames += numFrames;
        }
        return numFrames;
    }

    /**
     * Write numFrames from the float buffer into the recording, if there is room.
     * @param buffer
     * @param numFrames
     * @return number of frames actually written.
     */
    int32_t write(float *buffer, int32_t numFrames) {
        int32_t framesEmpty = mMaxFrames - mValidFrames;
        if (numFrames > framesEmpty) {
            numFrames = framesEmpty;
        }
        if (numFrames > 0) {
            int32_t numSamples = numFrames * mChannelCount;
            memcpy(mData + (mValidFrames * mChannelCount),
                   buffer,
                   (numSamples * sizeof(float)));
            mValidFrames += numFrames;
        }
        return numFrames;
    }

    /**
     * Read numFrames from the recording into the buffer, if there is enough data.
     * @param buffer
     * @param numFrames
     * @return number of frames actually read.
     */
    int32_t read(float *buffer, int32_t numFrames) {
        int32_t framesLeft = mValidFrames - mCursor;
        if (numFrames > framesLeft) {
            numFrames = framesLeft;
        }
        if (numFrames > 0) {
            int32_t numSamples = numFrames * mChannelCount;
            memcpy(buffer,
                   &mData[mCursor * mChannelCount],
                   (numSamples * sizeof(float)));
            mCursor += numFrames;
        }
        return numFrames;
    }

private:
    float          *mData = nullptr;
    int32_t         mValidFrames = 0;
    int32_t         mCursor = 0;
    const int32_t   mChannelCount;
    const int32_t   mMaxFrames;
};

#endif //NATIVEOBOE_MULTICHANNEL_RECORDING_H
