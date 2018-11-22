/*
 * Copyright 2016 The Android Open Source Project
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
#ifndef NATIVEOBOE_AUDIOGRAPHRUNNER_H
#define NATIVEOBOE_AUDIOGRAPHRUNNER_H

#include <unistd.h>
#include <sys/types.h>

#include "AudioProcessorBase.h"
#include "oboe/Oboe.h"

/**
 * Bridge between an audio graph and an audio device.
 * Connect the audio units to the "input" and then pass
 * this object to the AudioStreamBuilder as a callback.
 */
class AudioStreamGateway : public AudioProcessorBase, public oboe::AudioStreamCallback {
public:
    AudioStreamGateway(int samplesPerFrame);
    virtual ~AudioStreamGateway();

    /**
     * Process audio for the graph.
     * @param framePosition
     * @param numFrames
     * @return
     */
    AudioResult onProcess(
            uint64_t framePosition,
            int numFrames) override;

    /**
     * Called by Oboe when the stream is ready to process audio.
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int numFrames) override;

    AudioInputPort input;

    int getScheduler();

    void start() override {
        AudioProcessorBase::start();
        mCallCounter = 0;
        mFrameCountdown = 0;
    }

private:
    uint64_t mFramePosition;
    bool     mSchedulerChecked = false;
    int      mScheduler;

    int32_t  mCallCounter = 0;
    int32_t  mFrameCountdown = 0;
};


#endif //NATIVEOBOE_AUDIOGRAPHRUNNER_H
