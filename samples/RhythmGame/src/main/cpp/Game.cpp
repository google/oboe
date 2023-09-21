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
#include <cinttypes>

#include "Game.h"

Game::Game(AAssetManager &assetManager): mAssetManager(assetManager) {
}

void Game::load() {

    if (!openStream()) {
        mGameState = GameState::FailedToLoad;
        return;
    }

    if (!setupAudioSources()) {
        mGameState = GameState::FailedToLoad;
        return;
    }

    scheduleSongEvents();

    Result result = mAudioStream->requestStart();
    if (result != Result::OK){
        LOGE("Failed to start stream. Error: %s", convertToText(result));
        mGameState = GameState::FailedToLoad;
        return;
    }

    mGameState = GameState::Playing;
}

void Game::start() {

    // async returns a future, we must store this future to avoid blocking. It's not sufficient
    // to store this in a local variable as its destructor will block until Game::load completes.
    mLoadingResult = std::async(&Game::load, this);
}

void Game::stop(){

    if (mAudioStream != nullptr){
        mAudioStream->close();
    }
}

void Game::tap(int64_t eventTimeAsUptime) {

    if (mGameState != GameState::Playing){
        LOGW("Game not in playing state, ignoring tap event");
    } else {
        mClap->setPlaying(true);

        int64_t nextClapWindowTimeMs;
        if (mClapWindows.pop(nextClapWindowTimeMs)){

            // Convert the tap time to a song position
            int64_t tapTimeInSongMs = mSongPositionMs + (eventTimeAsUptime - mLastUpdateTime);
            TapResult result = getTapResult(tapTimeInSongMs, nextClapWindowTimeMs);
            mUiEvents.push(result);
        }
    }
}

void Game::tick(){

    switch (mGameState){
        case GameState::Playing:
            TapResult r;
            if (mUiEvents.pop(r)) {
                renderEvent(r);
            } else {
                SetGLScreenColor(kPlayingColor);
            }
            break;

        case GameState::Loading:
            SetGLScreenColor(kLoadingColor);
            break;

        case GameState::FailedToLoad:
            SetGLScreenColor(kLoadingFailedColor);
            break;
    }
}

void Game::onSurfaceCreated() {
    SetGLScreenColor(kLoadingColor);
}

void Game::onSurfaceChanged(int widthInPixels, int heightInPixels) {
}

void Game::onSurfaceDestroyed() {
}

DataCallbackResult Game::onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {

    float *outputBuffer = static_cast<float *>(audioData);
    int64_t nextClapEventMs;

    for (int i = 0; i < numFrames; ++i) {

        mSongPositionMs = convertFramesToMillis(
                mCurrentFrame,
                mAudioStream->getSampleRate());

        if (mClapEvents.peek(nextClapEventMs) && mSongPositionMs >= nextClapEventMs){
            mClap->setPlaying(true);
            mClapEvents.pop(nextClapEventMs);
        }
        mMixer.renderAudio(outputBuffer+(oboeStream->getChannelCount()*i), 1);
        mCurrentFrame++;
    }

    mLastUpdateTime = nowUptimeMillis();

    return DataCallbackResult::Continue;
}

void Game::onErrorAfterClose(AudioStream *oboeStream, Result error){
    if (error == Result::ErrorDisconnected){
        mGameState = GameState::Loading;
        mAudioStream.reset();
        mMixer.removeAllTracks();
        mCurrentFrame = 0;
        mSongPositionMs = 0;
        mLastUpdateTime = 0;
        start();
    } else {
        LOGE("Stream error: %s", convertToText(error));
    }
};

bool Game::openStream() {

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setFormat(AudioFormat::Float);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);
    builder.setCallback(this);
    builder.setSampleRate(48000);
    builder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);

    Result result = builder.openManagedStream(mAudioStream);
    if (result != Result::OK){
        LOGE("Failed to open stream. Error: %s", convertToText(result));
        return false;
    }

    if (mAudioStream->getFormat() != AudioFormat::Float){
        LOGE("The codelab version of this sample only supports floating point output."
             "Please check the master branch which contains sample format conversion");
        return false;
    }

    mMixer.setChannelCount(mAudioStream->getChannelCount());
    return true;
}

bool Game::setupAudioSources() {

    // Create a data source and player for the clap sound
    std::shared_ptr<AAssetDataSource> mClapSource {
            AAssetDataSource::newFromCompressedAsset(mAssetManager, "CLAP.mp3")
    };
    if (mClapSource == nullptr){
        LOGE("Could not load source data for clap sound");
        return false;
    }
    mClap = std::make_unique<Player>(mClapSource);

    // Create a data source and player for our backing track
    std::shared_ptr<AAssetDataSource> backingTrackSource {
            AAssetDataSource::newFromCompressedAsset(mAssetManager, "FUNKY_HOUSE.mp3")
    };
    if (backingTrackSource == nullptr){
        LOGE("Could not load source data for backing track");
        return false;
    }
    mBackingTrack = std::make_unique<Player>(backingTrackSource);
    mBackingTrack->setPlaying(true);
    mBackingTrack->setLooping(true);

    // Add both players to a mixer
    mMixer.addTrack(mClap.get());
    mMixer.addTrack(mBackingTrack.get());

    return true;
}

void Game::scheduleSongEvents() {

    // schedule the claps
    mClapEvents.push(0);
    mClapEvents.push(500);
    mClapEvents.push(1000);

    // schedule the clap windows
    mClapWindows.push(2000);
    mClapWindows.push(2500);
    mClapWindows.push(3000);

}

/**
 * Get the result of a tap
 *
 * @param tapTimeInMillis - The time the tap occurred in milliseconds
 * @param tapWindowInMillis - The time at the middle of the "tap window" in milliseconds
 * @return TapResult can be Early, Late or Success
 */
TapResult Game::getTapResult(int64_t tapTimeInMillis, int64_t tapWindowInMillis){
    LOGD("Tap time %" PRId64 ", tap window time: %" PRId64, tapTimeInMillis, tapWindowInMillis);
    if (tapTimeInMillis <= tapWindowInMillis + kWindowCenterOffsetMs) {
        if (tapTimeInMillis >= tapWindowInMillis - kWindowCenterOffsetMs) {
            return TapResult::Success;
        } else {
            return TapResult::Early;
        }
    } else {
        return TapResult::Late;
    }
}
