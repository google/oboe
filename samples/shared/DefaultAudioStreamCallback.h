/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef SAMPLES_DEFAULT_AUDIO_STREAM_CALLBACK_H
#define SAMPLES_DEFAULT_AUDIO_STREAM_CALLBACK_H


#include <oboe/AudioStreamCallback.h>
#include <logging_macros.h>

#include "IRenderableAudio.h"
#include "AudioEngine.h"

class DefaultAudioStreamCallback : public oboe::AudioStreamCallback {
public:
    virtual ~DefaultAudioStreamCallback() = default;

    virtual oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        float *outputBuffer = static_cast<float *>(audioData);
        if (!mRenderable) {
            LOGE("Renderable source not set!");
            return oboe::DataCallbackResult::Stop;
        }
        mRenderable->renderAudio(outputBuffer, numFrames);
        return oboe::DataCallbackResult::Continue;
    }
    virtual void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
        // Restart the stream when it errors out with disconnect
        if (error == oboe::Result::ErrorDisconnected && mParent) {
            LOGE("Restarting AudioStream after disconnect");
            mParent->restartStream();
        } else {
            LOGE("Could not find parent or unknown error");
        }
    }

    void setParent(AudioEngine &parent) {
        mParent = &parent;
    }

    void setSource(std::shared_ptr<IRenderableAudio> renderable) {
        mRenderable = renderable;
    }

    std::shared_ptr<IRenderableAudio> getSource() {
        return mRenderable;
    }

private:
    std::shared_ptr<IRenderableAudio> mRenderable;
    AudioEngine *mParent;
};

#endif //SAMPLES_DEFAULT_AUDIO_STREAM_CALLBACK_H
