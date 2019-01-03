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

#ifndef SHARED_MIXER_MONO_H
#define SHARED_MIXER_MONO_H

#include <array>
#include "IRenderableAudio.h"

constexpr int32_t kBufferSize = 192*10;  // Temporary buffer is used for mixing
constexpr uint8_t kMaxTracks = 100;

/**
 * A Mixer object which sums the output from multiple mono tracks into a single mono output
 */
class MixerMono : public IRenderableAudio {

public:
    void renderAudio(float *audioData, int32_t numFrames) {

        // Zero out the incoming container array
        memset(audioData, 0, sizeof(float) * numFrames);

        for (int i = 0; i < mNextFreeTrackIndex; ++i) {
            mTracks[i]->renderAudio(mixingBuffer, numFrames);

            for (int j = 0; j < numFrames; ++j) {
                audioData[j] += mixingBuffer[j];
            }
        }
    }

    void addTrack(std::shared_ptr<IRenderableAudio> renderer){
        mTracks[mNextFreeTrackIndex++] = renderer;
    }

private:
    float mixingBuffer[kBufferSize];
    std::array<std::shared_ptr<IRenderableAudio>, kMaxTracks> mTracks;
    uint8_t mNextFreeTrackIndex = 0;
};


#endif //SHARED_MIXER_MONO_H
