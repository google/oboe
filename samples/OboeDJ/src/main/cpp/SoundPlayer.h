/*
 * Copyright 2026 The Android Open Source Project
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

#ifndef OBOEDJ_SOUNDPLAYER_H
#define OBOEDJ_SOUNDPLAYER_H

#include <cstdint>
#include <memory>
#include <vector>
#include <math.h>
#include <player/SampleBuffer.h>

namespace oboedj {

/**
 * A player that reads from a pre-loaded PCM buffer with variable speed and linear interpolation.
 */
class SoundPlayer {
public:
    SoundPlayer(std::shared_ptr<iolib::SampleBuffer> buffer)
        : mBuffer(buffer), mPosition(0.0f), mSpeed(1.0f), mIsPlaying(false) {}

    void setSpeed(float speed) {
        mSpeed = speed;
    }

    void setPlaying(bool isPlaying) {
        mIsPlaying = isPlaying;
    }

    bool isPlaying() const {
        return mIsPlaying;
    }

    void reset() {
        mPosition = 0.0f;
    }

    /**
     * Render audio into the output buffer with linear interpolation for variable speed.
     */
    void renderAudio(float* outBuffer, int32_t numChannels, int32_t numFrames) {
        if (!mIsPlaying || !mBuffer) return;

        float* data = mBuffer->getSampleData();
        int32_t totalSamples = mBuffer->getNumSamples();
        int32_t bufferChannels = mBuffer->getProperties().channelCount;

        if (totalSamples == 0 || data == nullptr) return;

        int32_t framesAvailable = (totalSamples / bufferChannels);

        for (int32_t i = 0; i < numFrames; ++i) {
            float floatIndex = mPosition;
            int32_t index0 = static_cast<int32_t>(floatIndex);
            int32_t index1 = index0 + 1;

            float frac = floatIndex - index0;

            // Loop or clamp
            if (index0 >= framesAvailable) {
                mPosition = 0.0f; // Reset if looped
                break; // Or handle loop properly
            }
            if (index1 >= framesAvailable) {
                index1 = index0; // Clamp at edge
            }

            for (int32_t c = 0; c < numChannels; ++c) {
                // If buffer is mono and output is stereo, duplicate. If same, map.
                int32_t bufChannel = (c < bufferChannels) ? c : 0;
                
                float sample0 = data[index0 * bufferChannels + bufChannel];
                float sample1 = data[index1 * bufferChannels + bufChannel];

                // Linear interpolation
                float interpolated = sample0 * (1.0f - frac) + sample1 * frac;

                outBuffer[i * numChannels + c] += interpolated;
            }

            mPosition += mSpeed;
            while (mPosition >= framesAvailable) {
                mPosition -= framesAvailable;
            }
            while (mPosition < 0) {
                mPosition += framesAvailable;
            }
        }
    }

private:
    std::shared_ptr<iolib::SampleBuffer> mBuffer;
    float mPosition; // Fractional position in frames
    float mSpeed;    // 1.0 = normal, 2.0 = double speed, -1.0 = reverse
    bool mIsPlaying;
};

} // namespace oboedj

#endif // OBOEDJ_SOUNDPLAYER_H
