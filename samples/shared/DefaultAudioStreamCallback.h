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

#ifndef SAMPLES_DEFAULTAUDIOSTREAMCALLBACK_H
#define SAMPLES_DEFAULTAUDIOSTREAMCALLBACK_H


#include <oboe/AudioStreamCallback.h>
#include <shared/AudioEngineBase.h>
#include <shared/RenderableTap.h>
#include "../debug-utils/logging_macros.h"

class DefaultAudioStreamCallback : public oboe::AudioStreamCallback {
public:
    virtual oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        float *outputBuffer = static_cast<float *>(audioData);
        if (!mRenderable) {
            LOGE("mRenderable is NULL!");
            return oboe::DataCallbackResult::Stop;
        } else {
            mRenderable->renderAudio(outputBuffer, numFrames);
            return oboe::DataCallbackResult::Continue;
        }
    }
    virtual void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
        // Restart the stream when it errors out with disconnect
        if (error == oboe::Result::ErrorDisconnected) {
            LOGE("Restarting AudioStream after disconnect");
            mEnginePtr->restartStream();
        }
    }

    virtual void onSetupComplete() {}

private:
    RenderableTap *mRenderable;
    void setCallbackSource(RenderableTap *source) { mRenderable = source; }
    void setEnginePtr(AudioEngineBase *enginePtr) { mEnginePtr = enginePtr; }
    AudioEngineBase *mEnginePtr;
    template<class T1, class T2>
    friend class AudioEngine;
};

#endif //SAMPLES_DEFAULTAUDIOSTREAMCALLBACK_H
