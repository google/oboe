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
#include <audio/SoundRecording.h>
#include <audio/Mixer.h>
#include <utils/LockFreeQueue.h>
#include <utils/UtilityFunctions.h>

using namespace oboe;

class Game : public AudioStreamCallback {
public:
    Game(AAssetManager *assetManager);

    void start();
    void onSurfaceCreated();
    void onSurfaceDestroyed();
    void onSurfaceChanged(int widthInPixels, int heightInPixels);
    void tick();
    void tap(int64_t eventTimeAsUptime);

    DataCallbackResult
    onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    AAssetManager *mAssetManager;
    AudioStream *mAudioStream;
    SoundRecording *mClap;
    SoundRecording *mBackingTrack;
    Mixer mMixer;

    LockFreeQueue<int64_t, 1024> mClapEvents;
    LockFreeQueue<int64_t, 1024> mClapWindows;
    LockFreeQueue<TapResult, 1024> mUiEvents;
    std::atomic<int64_t> mCurrentFrame { 0 };
    std::atomic<int64_t> mLastUpdateTime { 0 };
};


#endif //RHYTHMGAME_GAME_H
