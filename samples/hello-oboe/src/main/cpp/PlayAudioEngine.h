/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef OBOE_HELLOOBOE_PLAYAUDIOENGINE_H
#define OBOE_HELLOOBOE_PLAYAUDIOENGINE_H

#include <thread>
#include <array>
#include <oboe/Oboe.h>
#include "SineGenerator.h"

constexpr int32_t kBufferSizeAutomatic = 0;
constexpr int32_t kMaximumChannelCount = 8;

class PlayAudioEngine : oboe::AudioStreamCallback {

public:
    PlayAudioEngine();

    ~PlayAudioEngine();

    void setAudioApi(oboe::AudioApi audioApi);

    void setDeviceId(int32_t deviceId);

    void setToneOn(bool isToneOn);

    void setBufferSizeInBursts(int32_t numBursts);

    double getCurrentOutputLatencyMillis();

    bool isLatencyDetectionSupported();

    // oboe::StreamCallback methods
    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error);

    void setChannelCount(int channelCount);

private:
    oboe::AudioApi mAudioApi = oboe::AudioApi::Unspecified;
    int32_t mPlaybackDeviceId = oboe::kUnspecified;
    int32_t mSampleRate;
    int32_t mChannelCount;
    bool mIsToneOn = false;
    int32_t mFramesPerBurst;
    double mCurrentOutputLatencyMillis = 0;
    int32_t mBufferSizeSelection = kBufferSizeAutomatic;
    bool mIsLatencyDetectionSupported = false;
    oboe::AudioStream *mPlayStream;
    std::unique_ptr<oboe::LatencyTuner> mLatencyTuner;
    std::mutex mRestartingLock;

    // The SineGenerators generate audio data, feel free to replace with your own audio generators
    std::array<SineGenerator, kMaximumChannelCount> mOscillators;

    void createPlaybackStream();

    void closeOutputStream();

    void restartStream();

    void setupPlaybackStreamParameters(oboe::AudioStreamBuilder *builder);

    void prepareOscillators();

    oboe::Result calculateCurrentOutputLatencyMillis(oboe::AudioStream *stream, double *latencyMillis);

};

#endif //OBOE_HELLOOBOE_PLAYAUDIOENGINE_H
