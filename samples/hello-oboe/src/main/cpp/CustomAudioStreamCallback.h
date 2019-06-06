//
// Created by atneya on 6/5/19.
//

#ifndef SAMPLES_CUSTOMAUDIOSTREAMCALLBACK_H
#define SAMPLES_CUSTOMAUDIOSTREAMCALLBACK_H

#include <shared/RenderableTap.h>
#include <shared/DefaultAudioStreamCallback.h>
#include <debug-utils/trace.h>

class CustomAudioStreamCallback : public DefaultAudioStreamCallback {

const int64_t kNanosPerMillisecond = 1000000;
public:
    oboe::DataCallbackResult
    /**
     * Every time the playback stream requires data this method will be called.
     *
     * @param audioStream the audio stream which is requesting data, this is the mPlayStream object
     * @param audioData an empty buffer into which we can write our audio data
     * @param numFrames the number of audio frames which are required
     * @return Either oboe::DataCallbackResult::Continue if the stream should continue requesting data
     * or oboe::DataCallbackResult::Stop if the stream should stop.
     */
    onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        if (bufferTuneEnabled && mLatencyTuner) mLatencyTuner->tune();
        auto underrunCountResult = oboeStream->getXRunCount();
        int32_t bufferSize = oboeStream->getBufferSizeInFrames();
        /**
         * The following output can be seen by running a systrace. Tracing is preferable to logging
        * inside the callback since tracing does not block.
        *
        * See https://developer.android.com/studio/profile/systrace-commandline.html
        */
        if (Trace::isEnabled()) Trace::beginSection("numFrames %d, Underruns %d, buffer size %d",
            numFrames, underrunCountResult.value(), bufferSize);

        auto result = DefaultAudioStreamCallback::onAudioReady(oboeStream, audioData, numFrames);
        if (latencyDetectionEnabled) calculateCurrentOutputLatencyMillis(oboeStream);
        if (Trace::isEnabled()) Trace::endSection();
        return result;
    }

    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
       DefaultAudioStreamCallback::onErrorAfterClose(oboeStream, error);
    }

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
    oboe::Result calculateCurrentOutputLatencyMillis(oboe::AudioStream *stream) {

        // Get the time that a known audio frame was presented for playing
        auto result = stream->getTimestamp(CLOCK_MONOTONIC);

        if (result == oboe::Result::OK) {

            oboe::FrameTimestamp playedFrame = result.value();

            // Get the write index for the next audio frame
            int64_t writeIndex = stream->getFramesWritten();

            // Calculate the number of frames between our known frame and the write index
            int64_t frameIndexDelta = writeIndex - playedFrame.position;

            // Calculate the time which the next frame will be presented
            int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) /  (stream->getSampleRate());
            int64_t nextFramePresentationTime = playedFrame.timestamp + frameTimeDelta;

            // Assume that the next frame will be written at the current time
            using namespace std::chrono;
            int64_t nextFrameWriteTime =
                    duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();

            // Calculate the latency
            *outputLatencyMillis = static_cast<double>(nextFramePresentationTime - nextFrameWriteTime)
                             / kNanosPerMillisecond;
        } else {
            LOGE("Error calculating latency: %s", oboe::convertToText(result.error()));
        }

        return result;
    }

};

#endif //SAMPLES_CUSTOMAUDIOSTREAMCALLBACK_H
