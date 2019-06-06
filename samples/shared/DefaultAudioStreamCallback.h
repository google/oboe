//
// Created by atneya on 6/4/19.
//

#ifndef SAMPLES_DEFAULTAUDIOSTREAMCALLBACK_H
#define SAMPLES_DEFAULTAUDIOSTREAMCALLBACK_H


#include <oboe/AudioStreamCallback.h>
#include <shared/RenderableTap.h>
#include "../debug-utils/logging_macros.h"

class DefaultAudioStreamCallback : public oboe::AudioStreamCallback {
public:

    virtual oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override{
        float *outputBuffer = static_cast<float*>(audioData);
        if (!mRenderable) {
            LOGE("mRendarable is NULL!");
            return oboe::DataCallbackResult ::Stop;
        } else {
            mRenderable->renderAudio(outputBuffer, numFrames);
            return oboe::DataCallbackResult::Continue;
        }
    }
    virtual void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
        // Restart the stream when it errors out with disconnect
        if (error == oboe::Result::ErrorDisconnected) {
            oboe::AudioStreamBuilder builder = {*oboeStream};
            LOGE("Restarting AudioStream after close");
            builder.openStream(&oboeStream);
        }
    }

    void setCallbackSource(RenderableTap *source) { mRenderable = source; }
private:
    RenderableTap *mRenderable;
};


#endif //SAMPLES_DEFAULTAUDIOSTREAMCALLBACK_H
