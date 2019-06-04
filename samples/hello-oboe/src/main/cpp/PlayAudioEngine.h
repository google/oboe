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

#include <oboe/Oboe.h>

#include "SoundGenerator.h"

constexpr int32_t kBufferSizeAutomatic = 0;

class PlayAudioEngine : oboe::AudioStreamCallback {

public:
    PlayAudioEngine();


    void setAudioApi(oboe::AudioApi audioApi);

    void setDeviceId(int32_t deviceId);

    void setChannelCount(int channelCount);

    void setBufferSizeInBursts(int32_t numBursts);

    void setToneOn(bool isToneOn);

    double getCurrentOutputLatencyMillis();

    bool isLatencyDetectionSupported();

    // oboe::StreamCallback methods
    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error);



private:
    oboe::ManagedStream mPlayStream;
    double mCurrentOutputLatencyMillis = 0;
    int32_t mBufferSizeSelection = kBufferSizeAutomatic; // Used to keep track if we are auto tuning
    bool mIsLatencyDetectionSupported = false;
    std::unique_ptr<oboe::LatencyTuner> mLatencyTuner;
    std::unique_ptr<SoundGenerator> mSoundGenerator;
    std::unique_ptr<float[]> mConversionBuffer { nullptr };
    // We will handle conversion to avoid getting kicked off the fast track as penalty

    void createPlaybackStream(oboe::AudioStreamBuilder *builder);

    void restartStream(oboe::AudioStreamBuilder *builder);

    oboe::Result calculateCurrentOutputLatencyMillis(oboe::AudioStream *stream, double *latencyMillis);

};

#endif //OBOE_HELLOOBOE_PLAYAUDIOENGINE_H
