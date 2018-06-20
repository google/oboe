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


#include <memory>
#include "AudioEngine.h"
#include "../../../../../src/common/OboeDebug.h"

void AudioEngine::start() {

    //LOGD("In start()");

    AudioStreamBuilder builder;
    builder.setCallback(this);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);

    Result result = builder.openStream(&mStream);
    if (result != Result::OK){
        LOGE("Failed to open stream. Error: %s", convertToText(result));
        return;
    }

    if (mStream->getFormat() == AudioFormat::Float){
        mSynth = std::make_unique<Synth<float>>(mStream->getSampleRate(), mStream->getChannelCount());
    } else {
        mSynth = std::make_unique<Synth<int16_t>>(mStream->getSampleRate(), mStream->getChannelCount());
    }

    mStream->setBufferSizeInFrames(mStream->getFramesPerBurst() * 2);
    mStream->requestStart();

    //LOGD("Finished start()");
}

void AudioEngine::stop() {

    //LOGD("In stop()");

    if (mStream != nullptr){
        mStream->close();
    }
    //LOGD("Finished stop()");
}

void AudioEngine::tap(bool isOn) {
    mSynth->setWaveOn(isOn);
}

DataCallbackResult
AudioEngine::onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    mSynth->renderAudio(audioData, numFrames);
    return DataCallbackResult::Continue;
}

