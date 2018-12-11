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

#include "Mixer.h"

void Mixer::renderAudio(int16_t *audioData, int32_t numFrames) {

    // Zero out the incoming container array
    for (int j = 0; j < numFrames * kChannelCount; ++j) {
        audioData[j] = 0;
    }

    for (int i = 0; i < mNextFreeTrackIndex; ++i) {
        mTracks[i]->renderAudio(mixingBuffer.data(), numFrames);

        for (int j = 0; j < numFrames * kChannelCount; ++j) {
            audioData[j] += mixingBuffer[j];
        }
    }
}

void Mixer::addTrack(std::shared_ptr<RenderableAudio> renderer){
    mTracks[mNextFreeTrackIndex++] = renderer;
    // If we've reached our track limit then overwrite the first track
    if (mNextFreeTrackIndex >= kMaxTracks)
        mNextFreeTrackIndex = 0;
};