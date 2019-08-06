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

#ifndef SAMPLES_AUDIO_ENGINE_H
#define SAMPLES_AUDIO_ENGINE_H


#include <oboe/Oboe.h>


class DefaultAudioStreamCallback;

class AudioEngine {
public:

   template <class callback_ptr>
    AudioEngine(std::shared_ptr<callback_ptr> callback) {
        mCallback = std::dynamic_pointer_cast<oboe::AudioStreamCallback>(callback);
        createPlaybackStream(this->configureBuilder());
    }

    virtual ~AudioEngine() = default;

    oboe::Result restartStream() {
        this->createPlaybackStream(this->configureBuilder());
        return startPlaybackStream();
    }

    virtual oboe::Result startPlaybackStream() {
        auto startResult = mStream->requestStart();
        return startResult;
    }

protected:
    oboe::ManagedStream mStream;
    std::shared_ptr<oboe::AudioStreamCallback> mCallback;

    // Default config properties of the stream, can be changed
    // These properties will be used on start and restart on disconnect
    virtual oboe::AudioStreamBuilder configureBuilder() {
        oboe::AudioStreamBuilder builder;
        builder.setSampleRate(48000)->setChannelCount(2);
        return builder;
    }

    void createCustomStream(oboe::AudioStreamBuilder builder) {
        createPlaybackStream(std::forward<decltype(builder)>(builder));
    }

private:
    oboe::Result createPlaybackStream(oboe::AudioStreamBuilder builder) {
        oboe::Result result = builder.setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setFormat(oboe::AudioFormat::Float)
            ->setCallback(mCallback.get())
            ->openManagedStream(mStream);
        return result;
    }
};

#endif //SAMPLES_AUDIO_ENGINE_H