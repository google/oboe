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

#ifndef RHYTHMGAME_GAME_H
#define RHYTHMGAME_GAME_H

#include <android/asset_manager.h>
#include <oboe/Oboe.h>

#include "shared/Mixer.h"

#include "audio/Player.h"
#include "audio/AAssetDataSource.h"
#include "ui/OpenGLFunctions.h"
#include "utils/LockFreeQueue.h"
#include "utils/UtilityFunctions.h"
#include "GameConstants.h"

using namespace oboe;

class Game : public AudioStreamCallback {
public:
    explicit Game(AAssetManager&);

    void start();
    void stop();
    void onSurfaceCreated();
    void onSurfaceDestroyed();
    void onSurfaceChanged(int widthInPixels, int heightInPixels);
    void tick();
    void tap(int64_t eventTimeAsUptime);

    // Inherited from oboe::AudioStreamCallback
    DataCallbackResult
    onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    AAssetManager& mAssetManager;
    AudioStream *mAudioStream{nullptr};
    std::shared_ptr<Player> mClap;
    std::shared_ptr<Player> mBackingTrack;
    Mixer<int16_t> mMixer;

    LockFreeQueue<int64_t, kMaxQueueItems> mClapEvents;
    std::atomic<int64_t> mCurrentFrame { 0 };
    LockFreeQueue<int64_t, kMaxQueueItems> mClapWindows;
    LockFreeQueue<TapResult, kMaxQueueItems> mUiEvents;
    std::atomic<int64_t> mLastUpdateTime { 0 };

};


#endif //RHYTHMGAME_GAME_H
