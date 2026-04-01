/*
 * Copyright 2026 The Android Open Source Project
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

#include "DJEngine.h"
#include <android/log.h>

static const char* TAG = "DJEngine";

namespace oboedj {

DJEngine::DJEngine() 
    : mCrossfader(0.5f) // Center
    , mChannelCount(2)
    , mSampleRate(48000) { // Default, will be updated
    
    // Initialize two empty decks
    mDecks.push_back(std::make_shared<Deck>(nullptr));
    mDecks.push_back(std::make_shared<Deck>(nullptr));
}

DJEngine::~DJEngine() {
    stopStream();
}

bool DJEngine::openStream() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mStream) return true; // Already open

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(mChannelCount)
           ->setDataCallback(this);

    oboe::Result result = builder.openStream(mStream);
    if (result != oboe::Result::OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open stream: %s", oboe::convertToText(result));
        return false;
    }

    mSampleRate = mStream->getSampleRate();
    __android_log_print(ANDROID_LOG_INFO, TAG, "Stream opened with sample rate: %d", mSampleRate);

    return true;
}

bool DJEngine::startStream() {
    std::lock_guard<std::mutex> lock(mLock);
    if (!mStream) return false;
    
    if (mStream->getState() == oboe::StreamState::Started) return true;

    oboe::Result result = mStream->requestStart();
    if (result != oboe::Result::OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to start stream: %s", oboe::convertToText(result));
        return false;
    }
    return true;
}

void DJEngine::stopStream() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mStream) {
        mStream->stop();
        mStream->close();
        mStream.reset();
    }
}

void DJEngine::loadTrack(uint8_t* buffer, int32_t length, int32_t deckIndex) {
    if (deckIndex < 0 || deckIndex >= mDecks.size()) return;

    parselib::MemInputStream stream(buffer, length);
    parselib::WavStreamReader reader(&stream);
    reader.parse();

    auto sampleBuffer = std::make_shared<iolib::SampleBuffer>();
    sampleBuffer->loadSampleData(&reader);
    
    // Statically resample to device rate if needed
    sampleBuffer->resampleData(mSampleRate);

    // Update the deck
    mDecks[deckIndex] = std::make_shared<Deck>(sampleBuffer);
    __android_log_print(ANDROID_LOG_INFO, TAG, "Loaded track for deck %d", deckIndex);
}

void DJEngine::setDeckSpeed(int32_t deckIndex, float speed) {
    if (deckIndex >= 0 && deckIndex < mDecks.size()) {
        mDecks[deckIndex]->setSpeed(speed);
    }
}

void DJEngine::setDeckPlaying(int32_t deckIndex, bool isPlaying) {
    if (deckIndex >= 0 && deckIndex < mDecks.size()) {
        mDecks[deckIndex]->setPlaying(isPlaying);
    }
}

void DJEngine::setCrossfader(float position) {
    mCrossfader.store(position);
}

oboe::DataCallbackResult DJEngine::onAudioReady(oboe::AudioStream *oboeStream, 
                                                void *audioData, 
                                                int32_t numFrames) {
    
    float* floatData = static_cast<float*>(audioData);
    int32_t numChannels = oboeStream->getChannelCount();

    // Clear buffer first
    memset(audioData, 0, numFrames * numChannels * sizeof(float));

    float crossfade = mCrossfader.load();
    float leftVolume = 1.0f - crossfade;
    float rightVolume = crossfade;

    // We need a temp buffer for each deck to scale volume
    std::vector<float> tempBuffer(numFrames * numChannels, 0.0f);

    for (size_t i = 0; i < mDecks.size(); ++i) {
        if (mDecks[i]->isPlaying()) {
            std::fill(tempBuffer.begin(), tempBuffer.end(), 0.0f);
            mDecks[i]->renderAudio(tempBuffer.data(), numChannels, numFrames);

            float volume = (i == 0) ? leftVolume : rightVolume;

            for (int32_t j = 0; j < numFrames * numChannels; ++j) {
                floatData[j] += tempBuffer[j] * volume;
            }
        }
    }

    return oboe::DataCallbackResult::Continue;
}

} // namespace oboedj
