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

#include "Synth.h"
#include "shared/TapAudioEngine.h"

using namespace oboe;

class AudioEngine : public TapAudioEngine<Synth> {

public:
    AudioEngine(std::vector<int> cpuIds);

protected:
    void createPlaybackStream(oboe::AudioStreamBuilder &builder) override {
        TapAudioEngine::createPlaybackStream(builder);
    }

private:
    std::vector<int> mCpuIds; // IDs of CPU cores which the audio callback should be bound to
    bool mIsThreadAffinitySet = false;
    void setThreadAffinity();
};


#endif //MEGADRONE_AUDIOENGINE_H
