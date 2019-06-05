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

#ifndef MEGADRONE_AUDIOENGINE_H
#define MEGADRONE_AUDIOENGINE_H


#include <oboe/Oboe.h>
#include <vector>
#include <debug-utils/logging_macros.h>

#include "Synth.h"

using namespace oboe;

class AudioEngine {

public:
    void start(std::vector<int> cpuIds);
    void tap(bool isOn);

private:
    ManagedStream mStream;
    std::unique_ptr<StabilizedCallback> mStabilizedCallback =
            std::make_unique<StabilizedCallback>(&mCallback);
    std::unique_ptr<Synth> mSynth;
    std::vector<int> mCpuIds; // IDs of CPU cores which the audio callback should be bound to
    bool mIsThreadAffinitySet = false;
    void setThreadAffinity();

private:
    struct : AudioStreamCallback {
    public:
        std::unique_ptr<Synth> mSynth;

        DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {
            float *outputBuffer = static_cast<float*>(audioData);
            mSynth->renderAudio(outputBuffer, numFrames);
            return DataCallbackResult::Continue;
        }
        void onErrorAfterClose(AudioStream *oboeStream, Result error) {
            // Restart the stream when it errors out
            AudioStreamBuilder builder = {*oboeStream};
            LOGE("Restarting AudioStream after close");
            builder.openStream(&oboeStream);
        }
    } mCallback;
};


#endif //MEGADRONE_AUDIOENGINE_H
