/*
 * Copyright 2019 The Android Open Source Project
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

// wavlib includes
#include <io/stream/MemInputStream.h>
#include <io/wav/WavStreamReader.h>
#include <player/OneShotSampleBuffer.h>

#include "SimpleMultiPlayer.h"

static const char* TAG = "SimpleMultiPlayer";

using namespace wavlib;

constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)

SimpleMultiPlayer::SimpleMultiPlayer()
  : mChannelCount(0), mSampleRate(0), mOutputReset(false)
{}

DataCallbackResult SimpleMultiPlayer::onAudioReady(AudioStream *oboeStream, void *audioData,
        int32_t numFrames) {

    StreamState streamState = oboeStream->getState();
    if (streamState != StreamState::Open && streamState != StreamState::Started) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState:%d", streamState);
    }
    if (streamState == StreamState::Disconnected) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState::Disconnected");
    }

    memset(audioData, 0, numFrames * sizeof(float));

    for(int32_t index = 0; index < mNumSampleBuffers; index++) {
        if (mSampleBuffers[index].isPlaying()) {
            mSampleBuffers[index].mixAudio((float*)audioData, numFrames);
        }
    }

    return DataCallbackResult::Continue;
}

void SimpleMultiPlayer::onErrorAfterClose(AudioStream *oboeStream, Result error) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "==== onErrorAfterClose() error:%d", error);

    resetAll();
    openStream();
    mOutputReset = true;
}

void SimpleMultiPlayer::onErrorBeforeClose(AudioStream *, Result error) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "==== onErrorBeforeClose() error:%d", error);
}

bool SimpleMultiPlayer::openStream() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "openStream()");

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setChannelCount(mChannelCount);
    builder.setSampleRate(mSampleRate);
    builder.setCallback(this);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);
    builder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);

    Result result = builder.openStream(&mAudioStream);
    if (result != Result::OK){
        return false;
    }

    // Reduce stream latency by setting the buffer size to a multiple of the burst size
    auto setBufferSizeResult = mAudioStream->setBufferSizeInFrames(
            mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
    if (setBufferSizeResult != Result::OK) {
        return false;
    }

    result = mAudioStream->start();
    if (result != Result::OK){
        return false;
    }

    return true;
}

void SimpleMultiPlayer::setupAudioStream(int32_t numSampleBuffers, int32_t channelCount, int32_t sampleRate) {

    mChannelCount = channelCount;
    mSampleRate = sampleRate;

    mSampleBuffers = new OneShotSampleBuffer[mNumSampleBuffers = numSampleBuffers];
    openStream();
}

void SimpleMultiPlayer::teardownAudioStream() {
    // tear down the player
    if (mAudioStream != nullptr) {
        mAudioStream->stop();
        delete mAudioStream;
        mAudioStream = nullptr;
    }

    // Unload the samples
    unloadSampleData();
}

void SimpleMultiPlayer::loadSampleDataFromAsset(byte* dataBytes, int32_t dataLen, int32_t index) {
    MemInputStream stream(dataBytes, dataLen);

    WavStreamReader reader(&stream);
    reader.parse();

    mSampleBuffers[index].loadSampleData(&reader);
}

void SimpleMultiPlayer::unloadSampleData() {
    for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        mSampleBuffers[bufferIndex].unloadSampleData();
    }
    delete[] mSampleBuffers;
    mSampleBuffers = nullptr;
    mNumSampleBuffers = 0;
}

void SimpleMultiPlayer::triggerDown(int32_t index) {
    if (index < mNumSampleBuffers) {
        mSampleBuffers[index].setPlayMode();
    }
}

void SimpleMultiPlayer::triggerUp(int32_t index) {
    if (index < mNumSampleBuffers) {
        mSampleBuffers[index].setStopMode();
    }
}

void SimpleMultiPlayer::resetAll() {
    for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        mSampleBuffers[bufferIndex].setStopMode();
    }
}
