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

void
PowerPlayMultiPlayer::MyPresentationCallback::onPresentationEnded(oboe::AudioStream *oboeStream) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "==== MyPresentationCallback() called with gain %f",
                        mParent->getGain(0));
}

void PowerPlayMultiPlayer::setupAudioStream(int32_t channelCount,
                                            oboe::PerformanceMode performanceMode) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "setupAudioStream()");
    mChannelCount = channelCount;

    openStream(performanceMode);
}

bool PowerPlayMultiPlayer::openStream(oboe::PerformanceMode performanceMode) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "openStream()");
    mLastPerformanceMode = performanceMode;

    // Use shared_ptr to prevent use of a deleted callback.
    mDataCallback = std::make_shared<MyDataCallback>(this);
    mErrorCallback = std::make_shared<MyErrorCallback>(this);
    mPresentationCallback = std::make_shared<MyPresentationCallback>(this);

    // Create an audio stream
    AudioStreamBuilder builder;
    // TODO - Read sample rate, format from the file instead of hardcoding.
    builder.setChannelCount(mChannelCount)
            ->setDataCallback(mDataCallback)
            ->setErrorCallback(mErrorCallback)
            ->setPresentationCallback(mPresentationCallback)
            ->setFormat(AudioFormat::Float)
            ->setSampleRate(48000)
            ->setPerformanceMode(performanceMode)
            ->setFramesPerDataCallback(128)
            ->setSharingMode(SharingMode::Exclusive);

    Result result = builder.openStream(mAudioStream);
    if (result != Result::OK) {
        __android_log_print(
                ANDROID_LOG_ERROR,
                TAG,
                "openStream failed. Error: %s", convertToText(result));
        return false;
    }

    if (mAudioStream->getPerformanceMode() != oboe::PerformanceMode::PowerSavingOffloaded ||
        !OboeExtensions::isMMapUsed(mAudioStream.get())) {
        constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)
        result = mAudioStream->setBufferSizeInFrames(
                mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
        if (result != Result::OK) {
            __android_log_print(
                    ANDROID_LOG_WARN,
                    TAG,
                    "setBufferSizeInFrames failed. Error: %s", convertToText(result)
            );
        }
    }

    mSampleRate = mAudioStream->getSampleRate();
    return true;
}


void PowerPlayMultiPlayer::triggerUp(int32_t index) {
    if (index >= 0 && index < mNumSampleBuffers) {
        mSampleSources[index]->setStopMode(true);
    }
    if (mAudioStream) {
        mAudioStream->pause();
    }
}

void PowerPlayMultiPlayer::triggerDown(int32_t index, oboe::PerformanceMode performanceMode) {
    auto performanceModeChanged = performanceMode != mLastPerformanceMode;
    if (index >= 0 && index < mNumSampleBuffers) {
        mSampleSources[index]->setPlayMode(performanceModeChanged);
    }

    if (performanceModeChanged) {
        teardownAudioStream();
        if (!openStream(performanceMode)) {
            __android_log_print(ANDROID_LOG_ERROR,
                                TAG,
                                "Failed to reopen stream with new performance mode");
            return;
        }
    }
    if (mAudioStream) {
        startStream();
    }
}

bool PowerPlayMultiPlayer::setMMapEnabled(bool enabled) {
    auto result = oboe::OboeExtensions::setMMapEnabled(enabled);
    return result == 0;
}

bool PowerPlayMultiPlayer::isMMapEnabled() {
    return oboe::OboeExtensions::isMMapEnabled();
}

bool PowerPlayMultiPlayer::isMMapSupported() {
    return oboe::OboeExtensions::isMMapSupported();
}
