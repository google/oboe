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

#include <oboe/LatencyTuner.h>
#include <shared/RenderableTap.h>
#include <shared/DefaultAudioStreamCallback.h>
#include <debug-utils/trace.h>
#include <oboe/Oboe.h>

class LatencyTuningCallback: public DefaultAudioStreamCallback {
    const int64_t kNanosPerMillisecond = 1000000;
public:
    using DefaultAudioStreamCallback::DefaultAudioStreamCallback;

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

    void setLatencyDetectionEnabled(bool enabled) {latencyDetectionEnabled = enabled;}
    void setBufferTuneEnabled(bool enabled) {bufferTuneEnabled = enabled;}
    void setLatencyTuner(oboe::LatencyTuner *latencyTuner) {mLatencyTuner = latencyTuner;}
    void setOutputLatencyMillis(double *outputLatency) {outputLatencyMillis = outputLatency;}
private:
    bool latencyDetectionEnabled;
    bool bufferTuneEnabled = true;
    double *outputLatencyMillis;
    // Latency Tuner lives in callback, it should be exposed by Oboe.
    oboe::LatencyTuner *mLatencyTuner;
    /**
     * Calculate the current latency between writing a frame to the output stream and
     * the same frame being presented to the audio hardware.
     *
     * Here's how the calculation works:
     *
     * 1) Get the time a particular frame was presented to the audio hardware
     * @see AudioStream::getTimestamp
     * 2) From this extrapolate the time which the *next* audio frame written to the stream
     * will be presented
     * 3) Assume that the next audio frame is written at the current time
     * 4) currentLatency = nextFramePresentationTime - nextFrameWriteTime
     *
     * @param stream The stream being written to
     * @param latencyMillis pointer to a variable to receive the latency in milliseconds between
     * writing a frame to the stream and that frame being presented to the audio hardware.
     * @return oboe::Result::OK or a oboe::Result::Error* value. It is normal to receive an error soon
     * after a stream has started because the timestamps are not yet available.
     */
    oboe::Result calculateCurrentOutputLatencyMillis(oboe::AudioStream *stream);
};

#endif //SAMPLES_LATENCY_TUNING_CALLBACK_H
