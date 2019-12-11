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

// wavlib includes
#include <io/stream/MemInputStream.h>
#include <io/wav/WavStreamReader.h>
#include <player/OneShotSampleBuffer.h>

#include "SimpleMultiPlayer.h"

static const char* TAG = "SimpleMultiPlayer";

using namespace wavlib;

constexpr int kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)

SimpleMultiPlayer::SimpleMultiPlayer() {}

DataCallbackResult SimpleMultiPlayer::onAudioReady(AudioStream *oboeStream, void *audioData,
        int32_t numFrames) {

    memset(audioData, 0, numFrames * sizeof(float));

    for(int index = 0; index < mNumSampleBuffers; index++) {
        if (mSampleBuffers[index].isPlaying()) {
            mSampleBuffers[index].mixAudio((float*)audioData, numFrames);
        }
    }

    return DataCallbackResult::Continue;
}

void SimpleMultiPlayer::onErrorAfterClose(AudioStream *oboeStream, Result error) {

}

bool SimpleMultiPlayer::openStream(int channelCount, int sampleRate) {
    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setChannelCount(channelCount);
    builder.setSampleRate(sampleRate);
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

void SimpleMultiPlayer::setupAudioStream(int numSampleBuffers, int channelCount, int sampleRate) {
    mSampleBuffers = new OneShotSampleBuffer[mNumSampleBuffers = numSampleBuffers];
    openStream(channelCount, sampleRate);
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

void SimpleMultiPlayer::loadSampleDataFromAsset(byte* dataBytes, int dataLen, int index) {
    MemInputStream stream(dataBytes, dataLen);

    WavStreamReader reader(&stream);
    reader.parse();

    mSampleBuffers[index].loadSampleData(&reader);
}

void SimpleMultiPlayer::unloadSampleData() {
    for (int bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        mSampleBuffers[bufferIndex].unloadSampleData();
    }
    delete[] mSampleBuffers;
    mSampleBuffers = nullptr;
    mNumSampleBuffers = 0;
}

void SimpleMultiPlayer::triggerDown(int index) {
    if (index < mNumSampleBuffers) {
        mSampleBuffers[index].setPlayMode();
    }
}

void SimpleMultiPlayer::triggerUp(int index) {
    if (index < mNumSampleBuffers) {
        mSampleBuffers[index].setStopMode();
    }
}
