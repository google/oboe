/**
 * Copyright 2017 The Android Open Source Project
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


#ifndef SAMPLES_LATENCY_TUNING_CALLBACK_H
#define SAMPLES_LATENCY_TUNING_CALLBACK_H

#include <oboe/Oboe.h>
#include <oboe/LatencyTuner.h>

#include <TappableAudioSource.h>
#include <DefaultAudioStreamCallback.h>
#include <trace.h>

class LatencyTuningCallback: public DefaultAudioStreamCallback {
public:
    LatencyTuningCallback(IRestartable &mParent) : DefaultAudioStreamCallback(mParent) {}
    /**
     * Every time the playback stream requires data this method will be called.
     *
     * @param audioStream the audio stream which is requesting data, this is the mPlayStream object
     * @param audioData an empty buffer into which we can write our audio data
     * @param numFrames the number of audio frames which are required
     * @return Either oboe::DataCallbackResult::Continue if the stream should continue requesting data
     * or oboe::DataCallbackResult::Stop if the stream should stop.
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

    void setBufferTuneEnabled(bool enabled) {mBufferTuneEnabled = enabled;}

private:
    bool mBufferTuneEnabled = true;
    // This will be used to automatically tune the buffer size of the stream, obtaining optimal latency
    // Latency Tuner should probably be built and exposed by Oboe
    // We can't create the latency tuner until the first callback,
    // but construction is cheap.
    // Every new stream has a new callback, so we can rely on a null check to update
    std::unique_ptr<oboe::LatencyTuner> mLatencyTuner;
};

#endif //SAMPLES_LATENCY_TUNING_CALLBACK_H
