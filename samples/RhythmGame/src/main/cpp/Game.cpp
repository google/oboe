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

constexpr int32_t kChannelCount = 2;

Game::Game(AAssetManager &assetManager): mAssetManager(assetManager) {
}

void Game::start() {

    // Load the RAW PCM data files for both the clap sound and backing track into memory.
    std::shared_ptr<AAssetDataSource> mClapSource(AAssetDataSource::newFromAssetManager(mAssetManager,
        "CLAP.raw",
        oboe::ChannelCount::Stereo));
    if (mClapSource == nullptr){
        LOGE("Could not load source data for clap sound");
        return;
    }
    mClap = std::make_shared<Player>(mClapSource);

    std::shared_ptr<AAssetDataSource> mBackingTrackSource(AAssetDataSource::newFromAssetManager(mAssetManager,
        "FUNKY_HOUSE.raw",
        oboe::ChannelCount::Stereo));
    if (mBackingTrackSource == nullptr){
        LOGE("Could not load source data for backing track");
        return;
    }
    mBackingTrack = std::make_shared<Player>(mBackingTrackSource);
    mBackingTrack->setPlaying(true);
    mBackingTrack->setLooping(true);

    // Add the clap and backing track sounds to a mixer so that they can be played together
    // simultaneously using a single audio stream.
    mMixer.addTrack(mClap);
    mMixer.addTrack(mBackingTrack);

    // Add the audio frame numbers on which the clap sound should be played to the clap event queue.
    // The backing track tempo is 120 beats per minute, which is 2 beats per second. At a sample
    // rate of 48000 frames per second this means a beat occurs every 24000 frames, starting at
    // zero. So the first 3 beats are: 0, 24000, 48000
    mClapEvents.push(0);
    mClapEvents.push(24000);
    mClapEvents.push(48000);

    // We want the user to tap on the screen exactly 4 beats after the first clap. 4 beats is
    // 96000 frames, so we just add 96000 to the above frame numbers.
    mClapWindows.push(96000);
    mClapWindows.push(120000);
    mClapWindows.push(144000);

    // Create a builder
    AudioStreamBuilder builder;
    builder.setFormat(AudioFormat::I16);
    builder.setChannelCount(kChannelCount);
    builder.setSampleRate(kSampleRateHz);
    builder.setCallback(this);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);

    Result result = builder.openStream(&mAudioStream);
    if (result != Result::OK){
        LOGE("Failed to open stream. Error: %s", convertToText(result));
    }

    // Reduce stream latency by setting the buffer size to a multiple of the burst size
    auto setBufferSizeResult = mAudioStream->setBufferSizeInFrames(
            mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
    if (setBufferSizeResult != Result::OK){
        LOGW("Failed to set buffer size. Error: %s", convertToText(setBufferSizeResult.error()));
    }

    result = mAudioStream->requestStart();
    if (result != Result::OK){
        LOGE("Failed to start stream. Error: %s", convertToText(result));
    }
}

void Game::stop(){

    if (mAudioStream != nullptr){
        mAudioStream->close();
        delete mAudioStream;
        mAudioStream = nullptr;
    }
}

void Game::tap(int64_t eventTimeAsUptime) {
    mClap->setPlaying(true);

    int64_t nextClapWindowFrame;
    if (mClapWindows.pop(nextClapWindowFrame)){

        int64_t frameDelta = nextClapWindowFrame - mCurrentFrame;
        int64_t timeDelta = convertFramesToMillis(frameDelta, kSampleRateHz);
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
        SetGLScreenColor(kScreenBackgroundColor);
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

