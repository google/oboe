/*
 * Copyright 2025 The Android Open Source Project
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
#include <android/log.h>
#include "PowerPlayMultiPlayer.h"

static const char *TAG = "PowerPlayMultiPlayer";

using namespace oboe;
using namespace parselib;

PowerPlayMultiPlayer::PowerPlayMultiPlayer() = default;

PowerPlayMultiPlayer::~PowerPlayMultiPlayer() = default;

void PowerPlayMultiPlayer::MyPresentationCallback::onPresentationEnded(oboe::AudioStream *oboeStream) {
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "==== MyPresentationCallback() called with gain %f",
                        mParent->getGain(0)
    );
}

void PowerPlayMultiPlayer::setupAudioStream(int32_t channelCount, oboe::PerformanceMode performanceMode) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "setupAudioStream()");
    mChannelCount = channelCount;

    openStream(performanceMode);
}

bool PowerPlayMultiPlayer::openStream(oboe::PerformanceMode performanceMode) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "openStream()");

    // Use shared_ptr to prevent use of a deleted callback.
    mDataCallback = std::make_shared<MyDataCallback>(this);
    mErrorCallback = std::make_shared<MyErrorCallback>(this);
    mPresentationCallback = std::make_shared<MyPresentationCallback>(this);

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setChannelCount(mChannelCount);
    builder.setDataCallback(mDataCallback);
    builder.setErrorCallback(mErrorCallback);
    builder.setPresentationCallback(mPresentationCallback);
    builder.setFormat(AudioFormat::Float);
    builder.setSampleRate(48000);
    builder.setPerformanceMode(performanceMode);
    builder.setFramesPerDataCallback(128);
    builder.setSharingMode(SharingMode::Exclusive);
    builder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);

    Result result = builder.openStream(mAudioStream);
    if (result != Result::OK) {
        __android_log_print(
                ANDROID_LOG_ERROR,
                TAG,
                "openStream failed. Error: %s", convertToText(result));
        return false;
    }

    if (!OboeExtensions::isMMapUsed(mAudioStream.get())) {
        constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)
        result = mAudioStream->setBufferSizeInFrames(mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
    }

    if (result != Result::OK) {
        __android_log_print(
                ANDROID_LOG_WARN,
                TAG,
                "setBufferSizeInFrames failed. Error: %s", convertToText(result)
        );
    }

    mSampleRate = mAudioStream->getSampleRate();
    return true;
}


void PowerPlayMultiPlayer::triggerUp(int32_t index) {
    mAudioStream->pause();
}

void PowerPlayMultiPlayer::triggerDown(int32_t index, oboe::PerformanceMode performanceMode) {
    if (index < mNumSampleBuffers) {
        mSampleSources[index]->setPlayMode();
    }

    if (!mAudioStream) setupAudioStream(mChannelCount, performanceMode);

    auto state = mAudioStream->getState();
    if (mAudioStream && state == StreamState::Closed) {
        openStream(performanceMode);
        startStream();
    }

    if (mAudioStream && state == StreamState::Open) startStream();
    if (mAudioStream && state == StreamState::Paused) mAudioStream->requestStart();
}
