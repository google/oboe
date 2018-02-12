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

#ifndef OBOE_OBOE_STREAM_H_
#define OBOE_OBOE_STREAM_H_

#include <stdint.h>
#include <time.h>
#include "oboe/OboeDefinitions.h"
#include "oboe/OboeStreamCallback.h"
#include "oboe/OboeStreamBase.h"

/** WARNING - UNDER CONSTRUCTION - THIS API WILL CHANGE. */

class OboeStreamBuilder;

#define DEFAULT_TIMEOUT_NANOS    (2000 * OBOE_NANOS_PER_MILLISECOND)

/**
 * Base class for Oboe C++ audio stream.
 */
class OboeStream : public OboeStreamBase {
public:

    OboeStream() {}
    explicit OboeStream(const OboeStreamBuilder &builder);

    virtual ~OboeStream() = default;

    /**
     * Open a stream based on the current settings.
     *
     * Note that we do not recommend re-opening a stream that has been closed.
     * TODO Should we prevent re-opening?
     *
     * @return
     */
    virtual oboe_result_t open();

    /**
     * Close the stream and deallocate any resources from the open() call.
     */
    virtual oboe_result_t close() = 0;

    /*
     * These are synchronous and will block until the operation is complete.
     */
    virtual oboe_result_t start(int64_t timeoutNanoseconds = DEFAULT_TIMEOUT_NANOS);
    virtual oboe_result_t pause(int64_t timeoutNanoseconds = DEFAULT_TIMEOUT_NANOS);
    virtual oboe_result_t flush(int64_t timeoutNanoseconds = DEFAULT_TIMEOUT_NANOS);
    virtual oboe_result_t stop(int64_t timeoutNanoseconds = DEFAULT_TIMEOUT_NANOS);

    /* Asynchronous requests.
     * Use waitForStateChange() if you need to wait for completion.
     */
    virtual oboe_result_t requestStart() = 0;
    virtual oboe_result_t requestPause() = 0;
    virtual oboe_result_t requestFlush() = 0;
    virtual oboe_result_t requestStop() = 0;

    /**
     * Query the current state, eg. OBOE_STREAM_STATE_PAUSING
     *
     * @return state or a negative error.
     */
    virtual oboe_stream_state_t getState() = 0;

    /**
     * Wait until the current state no longer matches the input state.
     * The current state is passed to avoid race conditions caused by the state
     * changing between calls.
     *
     * Note that generally applications do not need to call this. It is considered
     * an advanced technique.
     *
     * <pre><code>
     * int64_t timeoutNanos = 500 * OBOE_NANOS_PER_MILLISECOND; // arbitrary 1/2 second
     * oboe_stream_state_t currentState = stream->getState(stream);
     * while (currentState >= 0 && currentState != OBOE_STREAM_STATE_PAUSED) {
     *     currentState = stream->waitForStateChange(
     *                                   stream, currentState, timeoutNanos);
     * }
     * </code></pre>
     *
     * @param stream A handle provided by OboeStreamBuilder_openStream()
     * @param currentState The state we want to avoid.
     * @param nextState Pointer to a variable that will be set to the new state.
     * @param timeoutNanoseconds The maximum time to wait in nanoseconds.
     * @return OBOE_OK or a negative error.
     */
    virtual oboe_result_t waitForStateChange(oboe_stream_state_t currentState,
                                          oboe_stream_state_t *nextState,
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
    * @return resulting buffer size in frames or a negative error
    */
    virtual oboe_result_t setBufferSizeInFrames(int32_t requestedFrames) {
        return OBOE_ERROR_UNIMPLEMENTED;
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
    virtual int32_t getXRunCount() {
        return OBOE_ERROR_UNIMPLEMENTED;
    }

    /**
     * Query the number of frames that are read or written by the endpoint at one time.
     *
     * @return burst size
     */
    virtual int32_t getFramesPerBurst() = 0;

    bool isPlaying();

    OboeStreamCallback *getCallback() const { return mStreamCallback; }

    int32_t getBytesPerFrame() const { return mChannelCount * getBytesPerSample(); }

    int32_t getBytesPerSample() const;

    /**
     * This monotonic counter will never get reset.
     * @return the number of frames written so far
     */
    virtual int64_t getFramesWritten() { return mFramesWritten; }

    virtual int64_t getFramesRead() { return OBOE_ERROR_UNIMPLEMENTED; }

    virtual oboe_result_t getTimestamp(clockid_t clockId,
                                       int64_t *framePosition,
                                       int64_t *timeNanoseconds) {
        return OBOE_ERROR_UNIMPLEMENTED;
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
     * @return The number of frames actually written or a negative error.
     */
    virtual oboe_result_t write(const void *buffer,
                             int32_t numFrames,
                             int64_t timeoutNanoseconds) {
        return OBOE_ERROR_UNIMPLEMENTED;
    }

    virtual oboe_result_t read(void *buffer,
                            int32_t numFrames,
                            int64_t timeoutNanoseconds) {
        return OBOE_ERROR_UNIMPLEMENTED;
    }

    /**
     *
     * @return true if this stream is implemented using the AAudio API
     */
    virtual bool usesAAudio() const {
        return false;
    }

protected:

    virtual int64_t incrementFramesWritten(int32_t frames) {
        return mFramesWritten += frames;
    }

    /**
     * Wait for a transition from one state to another.
     * @return OBOE_OK if the endingState was observed, or OBOE_ERROR_UNEXPECTED_STATE
     *   if any state that was not the startingState or endingState was observed
     *   or OBOE_ERROR_TIMEOUT.
     */
    virtual oboe_result_t waitForStateTransition(oboe_stream_state_t startingState,
                                              oboe_stream_state_t endingState,
                                              int64_t timeoutNanoseconds);

    oboe_result_t fireCallback(void *audioData, int numFrames);

    virtual void setNativeFormat(oboe_audio_format_t format) {
        mNativeFormat = format;
    }

    // TODO make private
    // These do not change after open.
    oboe_audio_format_t  mNativeFormat = OBOE_AUDIO_FORMAT_INVALID;

private:
    int64_t              mFramesWritten = 0;
    int                  mPreviousScheduler = -1;
};


#endif /* OBOE_OBOE_STREAM_H_ */
