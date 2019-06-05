//
// Created by atneya on 6/4/19.
//

#ifndef SAMPLES_TAPAUDIOENGINE_H
#define SAMPLES_TAPAUDIOENGINE_H

#endif //SAMPLES_TAPAUDIOENGINE_H

#include <oboe/Oboe.h>
#include "shared/RenderableTap.h"
#include "shared/DefaultAudioStreamCallback.h"


// T is a RendarableTap Source, U is a Callback
template <class T, class U = DefaultAudioStreamCallback> class TapAudioEngine {
public:
    TapAudioEngine() {
        LOGD("Creating playback stream");
        oboe::AudioStreamBuilder builder;
        createPlaybackStream(builder);
    }
    void toggleTone() {
        isToneOn = !isToneOn;
        mSource->setToneOn(isToneOn);
        LOGD("Tone is %d", isToneOn);
    }
private:
    oboe::ManagedStream mStream;
    std::unique_ptr<RenderableTap> mSource;
    std::unique_ptr<DefaultAudioStreamCallback> mCallback = std::make_unique<DefaultAudioStreamCallback>();
    bool isToneOn = false;

    void createPlaybackStream(oboe::AudioStreamBuilder &builder) {
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
            LOGD("Stream state is, %d", mStream->getState());
            LOGD("Stream successfully started");
        } else {
            LOGE("Error starting stream. Error: %s", oboe::convertToText(result));
        }
    }

};