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
#include <shared/AudioEngine.h>

#include "SoundGenerator.h"
#include "LatencyTuningCallback.h"

constexpr int32_t kBufferSizeAutomatic = 0;

// This sample inherits the AudioEngine in the shared folder, with a custom audio source and callback
class PlayAudioEngine : public AudioEngine<SoundGenerator, LatencyTuningCallback> {

public:
    PlayAudioEngine();


    // These methods reset the underlying stream with new properties

    /**
     * Set the audio device which should be used for playback. Can be set to oboe::kUnspecified if
     * you want to use the default playback device (which is usually the built-in speaker if
     * no other audio devices, such as headphones, are attached).
     *
     * @param deviceId the audio device id, can be obtained through an {@link AudioDeviceInfo} object
     * using Java/JNI.
    */
    void setDeviceId(int32_t deviceId);

    void setChannelCount(int channelCount);

    void setAudioApi(oboe::AudioApi audioApi);

    void setBufferSizeInBursts(int32_t numBursts);

    // Used to display latency in the app

    double getCurrentOutputLatencyMillis();

    bool isLatencyDetectionSupported();

private:
    double mCurrentOutputLatencyMillis = 0;
    bool mIsLatencyDetectionSupported = false;
    std::unique_ptr<oboe::LatencyTuner> mLatencyTuner;
};

#endif //OBOE_HELLOOBOE_PLAYAUDIOENGINE_H
