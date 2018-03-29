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

#include <utils/logging.h>
#include <thread>

#include "Game.h"
#include "ui/OpenGLFunctions.h"

Game::Game(AAssetManager *assetManager): mAssetManager(assetManager) {
}

void Game::start() {

    mClap = SoundRecording::loadFromAssets(mAssetManager, "CLAP.raw");
    mBackingTrack = SoundRecording::loadFromAssets(mAssetManager, "FUNKY_HOUSE.raw" );
    mBackingTrack->setPlaying(true);
    mBackingTrack->setLooping(true);

    mMixer.addTrack(mClap);
    mMixer.addTrack(mBackingTrack);

    mClapEvents.push(0);
    mClapEvents.push(24000);
    mClapEvents.push(48000);

    mClapWindows.push(96000);
    mClapWindows.push(120000);
    mClapWindows.push(144000);

    AudioStreamBuilder builder;
    builder.setFormat(AudioFormat::I16);
    builder.setChannelCount(2);
    builder.setSampleRate(48000);
    builder.setCallback(this);

    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);

    builder.openStream(&mAudioStream);

    mAudioStream->setBufferSizeInFrames(mAudioStream->getFramesPerBurst() * 2);
    mAudioStream->requestStart();
}

void Game::tap(int64_t eventTimeAsUptime) {
    mClap->setPlaying(true);

    int64_t nextClapWindowFrame;
    if (mClapWindows.pop(nextClapWindowFrame)){

        int64_t frameDelta = nextClapWindowFrame - mCurrentFrame;
        int64_t timeDelta = convertFramesToMillis(frameDelta, 48000);
        int64_t windowTime = mLastUpdateTime + timeDelta;
        TapResult result = getTapResult(eventTimeAsUptime, windowTime);
        mUiEvents.push(result);
    }
}

void Game::tick(){

    TapResult r;
    if (mUiEvents.pop(r)) {
        renderEvent(r);
    } else {
        SetGLScreenColor(GREY);
    }
}

void Game::onSurfaceCreated() {
    SetGLScreenColor(GREY);
}

void Game::onSurfaceChanged(int widthInPixels, int heightInPixels) {
}

void Game::onSurfaceDestroyed() {
}

DataCallbackResult Game::onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {

    int64_t nextClapEvent;

    for (int i = 0; i < numFrames; ++i) {

        if (mClapEvents.peek(nextClapEvent) && mCurrentFrame == nextClapEvent){
            mClap->setPlaying(true);
            mClapEvents.pop(nextClapEvent);
        }
        mMixer.renderAudio(static_cast<int16_t*>(audioData)+(kChannelCount*i), 1);
        mCurrentFrame++;
    }

    mLastUpdateTime = nowUptimeMillis();

    return DataCallbackResult::Continue;
}

