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
#include <shared/TapAudioEngine.h>

#include "SoundGenerator.h"
#include "CustomAudioStreamCallback.h"

constexpr int32_t kBufferSizeAutomatic = 0;

class PlayAudioEngine : public TapAudioEngine<SoundGenerator, CustomAudioStreamCallback> {

public:
    PlayAudioEngine();

    void setAudioApi(oboe::AudioApi audioApi);

    void setDeviceId(int32_t deviceId);

    void setChannelCount(int channelCount);

    void setBufferSizeInBursts(int32_t numBursts);

    double getCurrentOutputLatencyMillis();

    bool isLatencyDetectionSupported();

protected:
    void createPlaybackStream(oboe::AudioStreamBuilder &builder) override; // This will delete old stream

private:
    double mCurrentOutputLatencyMillis = 0;
    bool mIsLatencyDetectionSupported = false;
    std::unique_ptr<oboe::LatencyTuner> mLatencyTuner;
};

#endif //OBOE_HELLOOBOE_PLAYAUDIOENGINE_H
