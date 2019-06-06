//
// Created by atneya on 6/4/19.
//

#ifndef SAMPLES_TAPAUDIOENGINE_H
#define SAMPLES_TAPAUDIOENGINE_H

#endif //SAMPLES_TAPAUDIOENGINE_H

#include <oboe/Oboe.h>
#include "shared/RenderableTap.h"
#include "shared/DefaultAudioStreamCallback.h"
#include "../debug-utils/logging_macros.h"

// T is a RenderableTap Source, U is a Callback
// The engine should be responsible for mutating and restarting the stream, as well as owning the callback

template <class T, class U = DefaultAudioStreamCallback> class TapAudioEngine {
public:
    TapAudioEngine() {
        LOGD("Creating playback stream");
        oboe::AudioStreamBuilder builder;
        this->createPlaybackStream(builder);
    }
    virtual ~TapAudioEngine() = default;
    void toggleTone() {
        isToneOn = !isToneOn;
        mSource->setToneOn(isToneOn);
    }
protected:
    virtual void createPlaybackStream(oboe::AudioStreamBuilder &builder) {
        oboe::Result result = builder.setSharingMode(oboe::SharingMode::Exclusive)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setCallback(mCallback.get())
                ->setFormat(oboe::AudioFormat::Float)
                ->openManagedStream(mStream);
        if (result == oboe::Result::OK) {
            mStream->setBufferSizeInFrames(mStream->getFramesPerBurst());
            mSource = std::make_unique<T>(mStream->getSampleRate(),
                    mStream->getBufferCapacityInFrames(),
                    mStream->getChannelCount());
            mCallback->setCallbackSource(mSource.get());
            auto startResult = mStream->requestStart();
            if (startResult != oboe::Result::OK) {
                LOGE("Error starting stream. %s", oboe::convertToText(result));
            }
            LOGW("Stream state is, %d", mStream->getState());
            LOGW("Stream successfully started");
        } else {
            LOGE("Error starting stream. Error: %s", oboe::convertToText(result));
        }
    }

    U* getCallbackPtr() {
        return dynamic_cast<U*>(mCallback.get());
    }

protected:
    oboe::ManagedStream mStream;
    std::unique_ptr<RenderableTap> mSource;
    std::unique_ptr<DefaultAudioStreamCallback> mCallback = std::make_unique<U>();
    bool isToneOn = false;



};