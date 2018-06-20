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

#include "Synth.h"

using namespace oboe;

class AudioEngine : public AudioStreamCallback {

public:
    void start();
    void tap(bool isOn);

    DataCallbackResult
    onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

    void stop();

private:

    AudioStream *mStream = nullptr;
    std::unique_ptr<ISynth> mSynth;

};


#endif //MEGADRONE_AUDIOENGINE_H
