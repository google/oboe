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

#include "oboe/OboeUtilities.h"
#include "OboeDebug.h"
#include "oboe/Oboe.h"

/*
 * OboeStream
 */

OboeStream::OboeStream(const OboeStreamBuilder &builder)
        : OboeStreamBase(builder) {
}

oboe_result_t OboeStream::open() {
    // TODO validate parameters or let underlyng API validate them?
    return OBOE_OK;
}

oboe_result_t OboeStream::fireCallback(void *audioData, int32_t numFrames)
{
    int scheduler = sched_getscheduler(0) & ~SCHED_RESET_ON_FORK; // for current thread
    if (scheduler != mPreviousScheduler) {
        LOGD("OboeStream::fireCallback() scheduler = %s",
             ((scheduler == SCHED_FIFO) ? "SCHED_FIFO" :
             ((scheduler == SCHED_OTHER) ? "SCHED_OTHER" :
             ((scheduler == SCHED_RR) ? "SCHED_RR" : "UNKNOWN")))
        );
        mPreviousScheduler = scheduler;
    }
    if (mStreamCallback == NULL) {
        return OBOE_ERROR_NULL;
    } else {
        oboe_result_t result = mStreamCallback->onAudioReady(this, audioData, numFrames);
        if (result == OBOE_OK) {
            mFramesWritten += numFrames;
        }
        return result;
    }
}

oboe_result_t OboeStream::waitForStateTransition(oboe_stream_state_t startingState,
                                               oboe_stream_state_t endingState,
                                               int64_t timeoutNanoseconds)
{
    oboe_stream_state_t state = getState();
    oboe_stream_state_t nextState = state;
    if (state == startingState && state != endingState) {
        oboe_result_t result = waitForStateChange(state, &nextState, timeoutNanoseconds);
        if (result < 0) {
            return result;
        }
    }
    if (nextState != endingState) {
        return OBOE_ERROR_INVALID_STATE;
    } else {
        return OBOE_OK;
    }
}

oboe_result_t OboeStream::start(int64_t timeoutNanoseconds)
{
    oboe_result_t result = requestStart();
    if (result < 0) return result;
    return waitForStateTransition(OBOE_STREAM_STATE_STARTING,
                                  OBOE_STREAM_STATE_STARTED, timeoutNanoseconds);
}

oboe_result_t OboeStream::pause(int64_t timeoutNanoseconds)
{
    oboe_result_t result = requestPause();
    if (result < 0) return result;
    return waitForStateTransition(OBOE_STREAM_STATE_PAUSING,
                                  OBOE_STREAM_STATE_PAUSED, timeoutNanoseconds);
}

oboe_result_t OboeStream::flush(int64_t timeoutNanoseconds)
{
    oboe_result_t result = requestFlush();
    if (result < 0) return result;
    return waitForStateTransition(OBOE_STREAM_STATE_FLUSHING,
                                  OBOE_STREAM_STATE_FLUSHED, timeoutNanoseconds);
}

oboe_result_t OboeStream::stop(int64_t timeoutNanoseconds)
{
    oboe_result_t result = requestStop();
    if (result < 0) return result;
    return waitForStateTransition(OBOE_STREAM_STATE_STOPPING,
                                  OBOE_STREAM_STATE_STOPPED, timeoutNanoseconds);
}

bool OboeStream::isPlaying() {
    oboe_stream_state_t state = getState();
    return state == OBOE_STREAM_STATE_STARTING || state == OBOE_STREAM_STATE_STARTED;
}

int32_t OboeStream::getBytesPerSample() const {
    return Oboe_convertFormatToSizeInBytes(mFormat);
}
