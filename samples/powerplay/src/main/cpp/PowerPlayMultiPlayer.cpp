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
    mLastMMapEnabled = isMMapEnabled();

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
    // Validate index is not out of bounds.
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

    // Attempt to pause audio if the stream is not already paused.
    if (mAudioStream->getState() != StreamState::Paused) {
        const auto result = mAudioStream->requestPause();
        if (result != Result::OK) {
            __android_log_print(ANDROID_LOG_ERROR,
                                TAG,
                                "Unable to pause the audio stream.");
            return;
        }
    }

    // Assure previous sample is stopped and the play head is reset to zero, avoiding the
    // currently playing index. Only allow the playback head to reset when the song has changed.
    const auto currentlyPlayingIndex = getCurrentlyPlayingIndex();
    if (currentlyPlayingIndex != -1) {
        mSampleSources[currentlyPlayingIndex]->setStopMode(true);
    }
}

void PowerPlayMultiPlayer::triggerDown(int32_t index, oboe::PerformanceMode performanceMode) {
    // Validate index is not out of bounds.
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

    updatePerformanceMode(performanceMode);

    // Assure previous sample is stopped and the play head is reset to zero, avoiding the
    // currently playing index. Only allow the playback head to reset when the song has changed.
    const auto currentlyPlayingIndex = getCurrentlyPlayingIndex();
    if (currentlyPlayingIndex != -1 && currentlyPlayingIndex != index) {
        mSampleSources[currentlyPlayingIndex]->setStopMode(false);
    }
    mSampleSources[index]->setPlayMode(false);

    const auto currentPerformanceMode = mAudioStream->getPerformanceMode();
    const auto isOffloaded = currentPerformanceMode == PerformanceMode::PowerSavingOffloaded;
    if (mSampleSources[index]) {
        const auto isPlayHeadAtStart = mSampleSources[index]->getPlayHeadPosition() == 0;

        if (isOffloaded && isPlayHeadAtStart) {
            const auto result = mAudioStream->flushFromFrame(FlushFromAccuracy::Undefined, 0);
            if (result != Result::OK) {
                __android_log_print(ANDROID_LOG_ERROR,
                                    TAG,
                                    "Failed to flush from frame. Error: %s",
                                    convertToText(result.error()));
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

void PowerPlayMultiPlayer::updatePerformanceMode(oboe::PerformanceMode performanceMode) {
    if (performanceMode != mLastPerformanceMode ||
        isMMapEnabled() != mLastMMapEnabled) {

        __android_log_print(ANDROID_LOG_INFO, TAG, "updatePerformanceMode: Reopening stream");
        teardownAudioStream();
        openStream(performanceMode);
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
    // TODO due to the use of drumthumper as a scaffold base, for now, we must assume that the
    //      sample that has a progress cursor head is the playing sample. Ideally, we can
    //      redo the engine so it no longer relies on drumthumper as a base.
    for (auto i = 0; i < mSampleSources.size(); ++i) {
        if (mSampleSources[i]->getPlayHeadPosition() > 0) return i;
    }

    // No source is currently playing.
    return -1;
}

int32_t PowerPlayMultiPlayer::setBufferSizeInFrames(int32_t requestedFrames) {
    if (mAudioStream == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "setBufferSizeInFrames: Stream is null");
        return static_cast<int32_t>(Result::ErrorNull);
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "Requesting buffer size: %d frames (Input: %d)",
                        requestedFrames, requestedFrames);

    const auto result = mAudioStream->setBufferSizeInFrames(requestedFrames);

    if (!result) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to set buffer size: %s",
                            convertToText(result.error()));
        return static_cast<int32_t>(result.error());
    }

    // Return the actual value granted by the hardware.
    __android_log_print(ANDROID_LOG_INFO, TAG, "Buffer size set to %d frames.", result.value());
    return result.value();
}

int32_t PowerPlayMultiPlayer::getBufferCapacityInFrames() {
    if (mAudioStream == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "getBufferCapacityInFrames: Stream is null");
        return static_cast<int32_t>(Result::ErrorNull);
    }

    return mAudioStream->getBufferCapacityInFrames();
}
