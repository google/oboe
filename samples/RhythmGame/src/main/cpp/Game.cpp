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

void Game::load(){

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setCallback(this);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);

    Result result = builder.openStream(&mAudioStream);
    if (result != Result::OK){
        LOGE("Failed to open stream. Error: %s", convertToText(result));
        return;
    }

    LOGD("Stream state: %s", convertToText(mAudioStream));

    if (mAudioStream->getFormat() == AudioFormat::I16){
        mConversionBuffer = std::make_unique<float[]>(
                (size_t)mAudioStream->getBufferCapacityInFrames() *
                mAudioStream->getChannelCount());
    }

    // Reduce stream latency by setting the buffer size to a multiple of the burst size
    auto setBufferSizeResult = mAudioStream->setBufferSizeInFrames(
            mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
    if (setBufferSizeResult != Result::OK){
        LOGW("Failed to set buffer size. Error: %s", convertToText(setBufferSizeResult.error()));
    }

    // Set the properties of our audio source(s) to match that of our audio stream
    AudioProperties targetProperties {
        .channelCount = mAudioStream->getChannelCount(),
        .sampleRate = mAudioStream->getSampleRate()
    };

    // Create a data source and player for the clap sound
    std::shared_ptr<AAssetDataSource> mClapSource {
        AAssetDataSource::newFromCompressedAsset(mAssetManager, "CLAP.mp3", targetProperties)
    };
    if (mClapSource == nullptr){
        LOGE("Could not load source data for clap sound");
        return;
    }
    mClap = std::make_shared<Player>(mClapSource);

    // Create a data source and player for our backing track
    std::shared_ptr<AAssetDataSource> backingTrackSource {
            AAssetDataSource::newFromCompressedAsset(mAssetManager, "FUNKY_HOUSE.mp3", targetProperties)
    };
    if (backingTrackSource == nullptr){
        LOGE("Could not load source data for backing track");
        return;
    }
    mBackingTrack = std::make_shared<Player>(backingTrackSource);
    mBackingTrack->setPlaying(true);
    mBackingTrack->setLooping(true);

    // Add both players to a mixer
    mMixer.addTrack(mClap);
    mMixer.addTrack(mBackingTrack);

    // The game will first demonstrate the pattern which the user should copy by "clapping" at
    // certain times during the song. We can specify these times here in milliseconds. Our backing
    // track has a tempo of 120 beats per minute, which is 2 beats per second. This means that a
    // pattern of 3 claps starting on the first beat would mean clapping at 0ms, 500ms and 1000ms
    mClapEvents.push(0);
    mClapEvents.push(500);
    mClapEvents.push(1000);

    // We now want the user to tap on the screen exactly 4 beats after the first clap so we add clap
    // windows at 2000ms, 2500ms and 3000ms (or 2, 2.5 and 3 seconds).
    mClapWindows.push(2000);
    mClapWindows.push(2500);
    mClapWindows.push(3000);

    // Start the stream
    result = mAudioStream->requestStart();
    if (result != Result::OK){
        LOGE("Failed to start stream. Error: %s", convertToText(result));
    }

    mIsLoading = false;
}

void Game::start() {

    // async returns a future, we must store this future to avoid blocking. It's not sufficient
    // to store this in a local variable as its destructor will block until Game::load completes.
    mLoadingResult = std::async(&Game::load, this);
}

void Game::stop(){

    if (mAudioStream != nullptr){
        mAudioStream->close();
        delete mAudioStream;
        mAudioStream = nullptr;
    }
}

void Game::tap(int64_t eventTimeAsUptime) {

    if (mIsLoading){
        LOGW("Game is loading, ignoring tap event");
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

    if (mIsLoading){
        SetGLScreenColor(YELLOW);
    } else {
        TapResult r;
        if (mUiEvents.pop(r)) {
            renderEvent(r);
        } else {
            SetGLScreenColor(kScreenBackgroundColor);
        }
    }
}

void Game::onSurfaceCreated() {
    SetGLScreenColor(kScreenBackgroundColor);
}

void Game::onSurfaceChanged(int widthInPixels, int heightInPixels) {
}

void Game::onSurfaceDestroyed() {
}

DataCallbackResult Game::onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {

    // If our audio stream is expecting 16-bit samples we need to render our floats into a separate
    // buffer then convert them into 16-bit ints
    bool is16Bit = (oboeStream->getFormat() == AudioFormat::I16);
    float *outputBuffer = (is16Bit) ? mConversionBuffer.get() : static_cast<float *>(audioData);

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

    if (is16Bit){
        oboe::convertFloatToPcm16(outputBuffer,
                                  static_cast<int16_t*>(audioData),
                                  numFrames * oboeStream->getChannelCount());
    }

    mLastUpdateTime = nowUptimeMillis();

    return DataCallbackResult::Continue;
}

void Game::onErrorAfterClose(AudioStream *oboeStream, Result error){
    LOGE("The audio stream was closed, please restart the game. Error: %s", convertToText(error));
};

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