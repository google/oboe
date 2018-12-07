/*
 * Copyright 2018 The Android Open Source Project
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

#include "Player.h"
#include "utils/logging.h"

void Player::renderAudio(int16_t *targetData, int32_t numFrames){

    if (mIsPlaying){

        int32_t totalFrames = mSource->getTotalFrames();
        const int16_t *data = mSource->getData();

        // Check whether we're about to reach the end of the recording
        if (!mIsLooping && mReadFrameIndex + numFrames >= totalFrames){
            numFrames = totalFrames - mReadFrameIndex;
            mIsPlaying = false;
        }

        for (int i = 0; i < numFrames; ++i) {
            for (int j = 0; j < mChannelCount; ++j) {
                targetData[(i*mChannelCount)+j] = data[(mReadFrameIndex*mChannelCount)+j];
            }

            // Increment and handle wraparound
            if (++mReadFrameIndex >= totalFrames) mReadFrameIndex = 0;
        }

    } else {
        // fill with zeros to output silence
        for (int i = 0; i < numFrames * mChannelCount; ++i) {
            targetData[i] = 0;
        }
    }
}