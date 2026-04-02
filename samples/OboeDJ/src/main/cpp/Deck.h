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

#ifndef OBOEDJ_DECK_H
#define OBOEDJ_DECK_H

#include <memory>
#include "SoundPlayer.h"

namespace oboedj {

class Deck {
public:
    Deck(std::shared_ptr<iolib::SampleBuffer> buffer) {
        mPlayer = std::make_shared<SoundPlayer>(buffer);
    }

    void setSpeed(float speed) {
        mPlayer->setSpeed(speed);
    }

    void setPlaying(bool isPlaying) {
        mPlayer->setPlaying(isPlaying);
    }

    bool isPlaying() const {
        return mPlayer->isPlaying();
    }

    void reset() {
        mPlayer->reset();
    }

    void renderAudio(float* outBuffer, int32_t numChannels, int32_t numFrames) {
        mPlayer->renderAudio(outBuffer, numChannels, numFrames);
    }

private:
    std::shared_ptr<SoundPlayer> mPlayer;
};

} // namespace oboedj

#endif // OBOEDJ_DECK_H
