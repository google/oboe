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

    const int32_t channelCount = mSource->getChannelCount();

    if (mIsPlaying){

        int32_t framesToRenderFromData = numFrames;
        int32_t totalSourceFrames = mSource->getTotalFrames();
        const int16_t *data = mSource->getData();

        // Check whether we're about to reach the end of the recording
        if (!mIsLooping && mReadFrameIndex + numFrames >= totalSourceFrames){
            framesToRenderFromData = totalSourceFrames - mReadFrameIndex;
            mIsPlaying = false;
        }

        for (int i = 0; i < framesToRenderFromData; ++i) {
            for (int j = 0; j < channelCount; ++j) {
                targetData[(i*channelCount)+j] = data[(mReadFrameIndex*channelCount)+j];
            }

            // Increment and handle wraparound
            if (++mReadFrameIndex >= totalSourceFrames) mReadFrameIndex = 0;
        }

        if (framesToRenderFromData < numFrames){
            // fill the rest of the buffer with silence
            renderSilence(&targetData[framesToRenderFromData], numFrames * channelCount);
        }

    } else {
        renderSilence(targetData, numFrames * channelCount);
    }
}

void Player::renderSilence(int16_t *start, int32_t numSamples){
    for (int i = 0; i < numSamples; ++i) {
        start[i] = 0;
    }
}