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

#ifndef RHYTHMGAME_MIXER_H
#define RHYTHMGAME_MIXER_H

#include "RenderableAudio.h"

constexpr int32_t kBufferSize = 192*10;  // Temporary buffer is used for mixing
constexpr uint8_t kMaxTracks = 100;

template <typename T>
class Mixer : public RenderableAudio<T> {

public:
    void renderAudio(T *audioData, int32_t numFrames) {

        // Zero out the incoming container array
        memset(audioData, 0, sizeof(T) * numFrames);

        for (int i = 0; i < mNextFreeTrackIndex; ++i) {
            mTracks[i]->renderAudio(mixingBuffer, numFrames);

            for (int j = 0; j < numFrames; ++j) {
                audioData[j] += mixingBuffer[j];
            }
        }
    }

    void addTrack(std::shared_ptr<RenderableAudio<T>> renderer){
        mTracks[mNextFreeTrackIndex++] = renderer;
    }

private:
    T mixingBuffer[kBufferSize];
    std::array<std::shared_ptr<RenderableAudio<T>>, kMaxTracks> mTracks;
    uint8_t mNextFreeTrackIndex = 0;
};


#endif //RHYTHMGAME_MIXER_H
