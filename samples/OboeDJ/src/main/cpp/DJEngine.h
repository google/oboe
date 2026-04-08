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

#ifndef OBOEDJ_DJENGINE_H
#define OBOEDJ_DJENGINE_H

#include <oboe/Oboe.h>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

// iolib/parselib includes
#include <player/SampleBuffer.h>
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>

#include "Deck.h"

namespace oboedj {

class DJEngine : public oboe::AudioStreamDataCallback {
public:
    DJEngine();
    virtual ~DJEngine();

    bool openStream();
    bool startStream();
    void stopStream();

    void loadTrack(uint8_t* buffer, int32_t length, int32_t deckIndex);
    
    void setDeckSpeed(int32_t deckIndex, float speed);
    void setDeckPlaying(int32_t deckIndex, bool isPlaying);
    void setCrossfader(float position); // 0.0 (Left) to 1.0 (Right)

    // From AudioStreamDataCallback
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, 
                                          void *audioData, 
                                          int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<std::shared_ptr<Deck>> mDecks;
    
    std::atomic<float> mCrossfader; // 0.0 to 1.0
    int32_t mChannelCount;
    int32_t mSampleRate;

    std::mutex mLock; // Protect stream operations
};

} // namespace oboedj

#endif // OBOEDJ_DJENGINE_H
