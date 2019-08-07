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

#ifndef MEGADRONE_ENGINE_H
#define MEGADRONE_ENGINE_H


#include <oboe/Oboe.h>
#include <vector>

#include "Synth.h"
#include <DefaultAudioStreamCallback.h>
#include <TappableAudioSource.h>
#include <IRestartable.h>

using namespace oboe;

class MegaDroneEngine : public IRestartable {

public:
    MegaDroneEngine(std::vector<int> cpuIds);

    virtual ~MegaDroneEngine() = default;

    void tap(bool isDown);

    // from IRestartable
    virtual void restart() override;

private:
    oboe::ManagedStream mStream;
    std::shared_ptr<TappableAudioSource> mAudioSource;
    std::unique_ptr<DefaultAudioStreamCallback> mCallback;

    oboe::Result createPlaybackStream();
    void createCallback(std::vector<int> cpuIds);
    void start();
};


#endif //MEGADRONE_ENGINE_H
