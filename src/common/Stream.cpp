/*
 * Copyright 2015 The Android Open Source Project
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

#include <sys/types.h>
#include <pthread.h>
#include <oboe/Stream.h>
#include "OboeDebug.h"
#include <oboe/Utilities.h>

namespace oboe {

/*
 * Stream
 */
Stream::Stream(const StreamBuilder &builder)
        : StreamBase(builder) {
}

Result Stream::open() {
    // TODO validate parameters or let underlyng API validate them?
    return Result::OK;
}

Result Stream::fireCallback(void *audioData, int32_t numFrames)
{
    int scheduler = sched_getscheduler(0) & ~SCHED_RESET_ON_FORK; // for current thread
    if (scheduler != mPreviousScheduler) {
        LOGD("Stream::fireCallback() scheduler = %s",
             ((scheduler == SCHED_FIFO) ? "SCHED_FIFO" :
             ((scheduler == SCHED_OTHER) ? "SCHED_OTHER" :
             ((scheduler == SCHED_RR) ? "SCHED_RR" : "UNKNOWN")))
        );
        mPreviousScheduler = scheduler;
    }
    if (mStreamCallback == NULL) {
        return Result::ErrorNull;
    } else {
        /**
         * TODO: onAudioRead doesn't return an Result, it returns either Continue or Stop
         * neither of which tells us whether an error occured. Figure out what to do here.
         */
        /*Result result = mStreamCallback->onAudioReady(this, audioData, numFrames);
        if (result == OBOE_OK) {
            mFramesWritten += numFrames;
        }*/
        mStreamCallback->onAudioReady(this, audioData, numFrames);
        mFramesWritten += numFrames;
        return Result::OK;
    }
}

Result Stream::waitForStateTransition(StreamState startingState,
                                               StreamState endingState,
                                               int64_t timeoutNanoseconds)
{
    StreamState state = getState();
    StreamState nextState = state;
    if (state == startingState && state != endingState) {
        Result result = waitForStateChange(state, &nextState, timeoutNanoseconds);
        if (result != Result::OK) {
            return result;
        }
    }
    if (nextState != endingState) {
        return Result::ErrorInvalidState;
    } else {
        return Result::OK;
    }
}

Result Stream::start(int64_t timeoutNanoseconds)
{
    Result result = requestStart();
    if (result != Result::OK) return result;
    return waitForStateTransition(StreamState::Starting,
                                  StreamState::Started, timeoutNanoseconds);
}

Result Stream::pause(int64_t timeoutNanoseconds)
{
    Result result = requestPause();
    if (result != Result::OK) return result;
    return waitForStateTransition(StreamState::Pausing,
                                  StreamState::Paused, timeoutNanoseconds);
}

Result Stream::flush(int64_t timeoutNanoseconds)
{
    Result result = requestFlush();
    if (result != Result::OK) return result;
    return waitForStateTransition(StreamState::Flushing,
                                  StreamState::Flushed, timeoutNanoseconds);
}

Result Stream::stop(int64_t timeoutNanoseconds)
{
    Result result = requestStop();
    if (result != Result::OK) return result;
    return waitForStateTransition(StreamState::Stopping,
                                  StreamState::Stopped, timeoutNanoseconds);
}

bool Stream::isPlaying() {
    StreamState state = getState();
    return state == StreamState::Starting || state == StreamState::Started;
}

int32_t Stream::getBytesPerSample() const {
    return convertFormatToSizeInBytes(mFormat);
}

} // namespace oboe