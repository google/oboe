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
    // Validate index is not out of bounds
    if (index < 0 || index >= mSampleSources.size()) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "triggerDown: Invalid index %d", index);
        return;
    }

    // Validate the audio stream is not null.
    if (mAudioStream == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "triggerDown: mAudioStream is null after attempting to open.");
        return;
    }

    // If the performance mode has changed, we need to reopen the stream.
    if (performanceMode != mLastPerformanceMode) {
        teardownAudioStream();

        // Attempt here to reopen the stream with the new performance mode.
        const auto result = openStream(performanceMode);
        if (!result) {
            // Something went wrong and the stream could not be reopened.
            __android_log_print(ANDROID_LOG_ERROR,
                                TAG,
                                "Failed to reopen stream with new performance mode");
            return;
        }
    }

    const auto currentPerformanceMode = mAudioStream->getPerformanceMode();
    const auto currentlyPlayingIndex = getCurrentlyPlayingIndex();

    // Assure all other loaded samples are stopped and the play head is reset to zero, avoiding the
    // currently playing index. Only allow the playback head to reset when the song has changed.
    for (size_t i = 0; i < mSampleSources.size(); ++i) {
        if (i != index) mSampleSources[i]->setStopMode(false);
        else mSampleSources[i]->setPlayMode(currentlyPlayingIndex == i);
    }

    const auto isOffloaded = currentPerformanceMode == PerformanceMode::PowerSavingOffloaded;
    if (mSampleSources[index]) { // Ensure the specific sample source is valid before accessing it
        const auto isPlayHeadAtStart = mSampleSources[index]->getPlayHeadPosition() == 0;

        if (isOffloaded && isPlayHeadAtStart) {
            const auto result = mAudioStream->flushFromFrame(FlushFromAccuracy::Undefined, 0);
            if (result != Result::OK) {
                __android_log_print(ANDROID_LOG_ERROR,
                                    TAG,
                                    "Failed to flush from frame. Error: %s",
                                    convertToText(result.error()));
                return;
            }
        }
    }

    // Attempt to play audio if the stream is not already playing.
    if (mAudioStream->getState() != StreamState::Started) {
        const auto result = mAudioStream->requestStart();
        if (result != Result::OK) {
            __android_log_print(ANDROID_LOG_ERROR,
                                TAG,
                                "Unable to start the audio stream.");
        }
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

int32_t PowerPlayMultiPlayer::getCurrentlyPlayingIndex() {
    for (auto i = 0; i < mSampleSources.size(); ++i) {
        if (mSampleSources[i]->isPlaying()) return i;
    }

    // No source is currently playing.
    return -1;
}