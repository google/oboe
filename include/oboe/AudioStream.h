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

#ifndef OBOE_STREAM_H_
#define OBOE_STREAM_H_

#include <cstdint>
#include <ctime>
#include "oboe/Definitions.h"
#include "oboe/ErrorOrValue.h"
#include "oboe/AudioStreamBuilder.h"
#include "oboe/AudioStreamBase.h"

/** WARNING - UNDER CONSTRUCTION - THIS API WILL CHANGE. */

namespace oboe {

constexpr int64_t kDefaultTimeoutNanos = (2000 * kNanosPerMillisecond);

/**
 * Base class for Oboe C++ audio stream.
 */
class AudioStream : public AudioStreamBase {
public:

    AudioStream() {}
    explicit AudioStream(const AudioStreamBuilder &builder);

    virtual ~AudioStream() = default;

    /**
     * Open a stream based on the current settings.
     *
     * Note that we do not recommend re-opening a stream that has been closed.
     * TODO Should we prevent re-opening?
     *
     * @return
     */
    virtual Result open();

    /**
     * Close the stream and deallocate any resources from the open() call.
     */
    virtual Result close() = 0;

    /*
     * These are synchronous and will block until the operation is complete.
     */
    virtual Result start(int64_t timeoutNanoseconds = kDefaultTimeoutNanos);
    virtual Result pause(int64_t timeoutNanoseconds = kDefaultTimeoutNanos);
    virtual Result flush(int64_t timeoutNanoseconds = kDefaultTimeoutNanos);
    virtual Result stop(int64_t timeoutNanoseconds = kDefaultTimeoutNanos);

    /* Asynchronous requests.
     * Use waitForStateChange() if you need to wait for completion.
     */
    virtual Result requestStart() = 0;
    virtual Result requestPause() = 0;
    virtual Result requestFlush() = 0;
    virtual Result requestStop() = 0;

    /**
     * Query the current state, eg. StreamState::Pausing
     *
     * @return state or a negative error.
     */
    virtual StreamState getState() = 0;

    /**
     * Wait until the stream's current state no longer matches the input state.
     * The input state is passed to avoid race conditions caused by the state
     * changing between calls.
     *
     * Note that generally applications do not need to call this. It is considered
     * an advanced technique.
     *
     * <pre><code>
     * int64_t timeoutNanos = 500 * kNanosPerMillisecond; // arbitrary 1/2 second
     * StreamState currentState = stream->getState();
     * StreamState nextState = StreamState::Unknown;
     * while (result == Result::OK && currentState != StreamState::Paused) {
     *     result = stream->waitForStateChange(
     *                                   currentState, &nextState, timeoutNanos);
     *     currentState = nextState;
     * }
     * </code></pre>
     *
     * @param inputState The state we want to avoid.
     * @param nextState Pointer to a variable that will be set to the new state.
     * @param timeoutNanoseconds The maximum time to wait in nanoseconds.
     * @return Result::OK or a Result::Error.
     */
    virtual Result waitForStateChange(StreamState inputState,
                                          StreamState *nextState,
                                          int64_t timeoutNanoseconds) = 0;

    /**
    * This can be used to adjust the latency of the buffer by changing
    * the threshold where blocking will occur.
    * By combining this with getXRunCount(), the latency can be tuned
    * at run-time for each device.
    *
    * This cannot be set higher than getBufferCapacity().
    *
    * @param requestedFrames requested number of frames that can be filled without blocking
    * @return resulting buffer size in frames or a Result::Error
    */
    virtual Result setBufferSizeInFrames(int32_t requestedFrames) {
        return Result::ErrorUnimplemented;
    }

    /**
     * An XRun is an Underrun or an Overrun.
     * During playing, an underrun will occur if the stream is not written in time
     * and the system runs out of valid data.
     * During recording, an overrun will occur if the stream is not read in time
     * and there is no place to put the incoming data so it is discarded.
     *
     * An underrun or overrun can cause an audible "pop" or "glitch".
     *
     * @return the count or negative error.
     */
    virtual int32_t getXRunCount() const {
        return static_cast<int32_t>(Result::ErrorUnimplemented);
    }

    /**
     * Query the number of frames that are read or written by the endpoint at one time.
     *
     * @return burst size
     */
    virtual int32_t getFramesPerBurst() = 0;

    bool isPlaying();

    int32_t getBytesPerFrame() const { return mChannelCount * getBytesPerSample(); }

    int32_t getBytesPerSample() const;

    /**
     * This monotonic counter will never get reset.
     * @return the number of frames written so far
     */
    virtual int64_t getFramesWritten() const { return mFramesWritten; }

    virtual int64_t getFramesRead() const { return mFramesRead; }

    /**
     * Calculate the latency of a stream based on getTimestamp().
     *
     * Output latency is the time it takes for a given frame to travel from the
     * app to some type of digital-to-analog converter. If the DAC is external, for example
     * in a USB interface or a TV connected by HDMI, then there may be additional latency
     * that the Android device is unaware of.
     *
     * Input latency is the time it takes to a given frame to travel from an analog-to-digital
     * converter (ADC) to the app.
     *
     * Note that the latency of an OUTPUT stream will increase abruptly when you write data to it
     * and then decrease slowly over time as the data is consumed.
     *
     * The latency of an INPUT stream will decrease abruptly when you read data from it
     * and then increase slowly over time as more data arrives.
     *
     * The latency of an OUTPUT stream is generally higher than the INPUT latency
     * because an app generally tries to keep the OUTPUT buffer full and the INPUT buffer empty.
     *
     * @return The latency in milliseconds and Result::OK, or a negative error.
     */
    virtual ErrorOrValue<double> calculateLatencyMillis() {
        return ErrorOrValue<double>(Result::ErrorUnimplemented);
    }

    virtual Result getTimestamp(clockid_t clockId,
                                int64_t *framePosition,
                                int64_t *timeNanoseconds) {
        return Result::ErrorUnimplemented;
    }

    // ============== I/O ===========================
    /**
     * A high level write that will wait until the write is complete or it runs out of time.
     * If timeoutNanoseconds is zero then this call will not wait.
     *
     * @param stream A stream created using OboeStream_Open().
     * @param buffer The address of the first sample.
     * @param numFrames Number of frames to write. Only complete frames will be written.
     * @param timeoutNanoseconds Maximum number of nanoseconds to wait for completion.
     * @return The number of frames actually written and Result::OK, or a negative error.
     */
    virtual ErrorOrValue<int32_t> write(const void *buffer,
                             int32_t numFrames,
                             int64_t timeoutNanoseconds) {
        return ErrorOrValue<int32_t>(Result::ErrorUnimplemented);
    }

    virtual ErrorOrValue<int32_t> read(void *buffer,
                            int32_t numFrames,
                            int64_t timeoutNanoseconds) {
        return ErrorOrValue<int32_t>(Result::ErrorUnimplemented);
    }

    /**
     *
     * @return the API that this stream uses
     */
    virtual AudioApi getAudioApi() const = 0;

    /**
     * @return true if this stream is implemented using the AAudio API
     */
    bool usesAAudio() const {
        return getAudioApi() == AudioApi::AAudio;
    }

    /**
     * Do not use this for production. This is only for debugging.
     * If you need to call this method then something is wrong.
     * If you think you need it for production then please let us know
     * so we can modify Oboe so that you don't need this.
     *
     * @return nullptr or a pointer to a stream from the system API
     */
    virtual void *getUnderlyingStream() const {
        return nullptr;
    }

protected:

    virtual int64_t incrementFramesWritten(int32_t frames) {
        return mFramesWritten += frames;
    }
    virtual int64_t incrementFramesRead(int32_t frames) {
        return mFramesRead += frames;
    }

    /**
     * Wait for a transition from one state to another.
     * @return OK if the endingState was observed, or ErrorUnexpectedState
     *   if any state that was not the startingState or endingState was observed
     *   or ErrorTimeout.
     */
    virtual Result waitForStateTransition(StreamState startingState,
                                          StreamState endingState,
                                          int64_t timeoutNanoseconds);

    /**
     * Override this to provide a default for when the application did not specify a callback.
     *
     * @param audioData
     * @param numFrames
     * @return result
     */
    virtual DataCallbackResult onDefaultCallback(void *audioData, int numFrames) {
        return DataCallbackResult::Stop;
    }

    DataCallbackResult fireCallback(void *audioData, int numFrames);

    virtual void setNativeFormat(AudioFormat format) {
        mNativeFormat = format;
    }

    // TODO: make private
    // These do not change after open.
    AudioFormat mNativeFormat = AudioFormat::Invalid;

private:
    // TODO these should be atomic like in AAudio
    int64_t              mFramesWritten = 0;
    int64_t              mFramesRead = 0;
    int                  mPreviousScheduler = -1;
};

} // namespace oboe

#endif /* OBOE_STREAM_H_ */
