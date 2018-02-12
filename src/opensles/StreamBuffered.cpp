/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "oboe/Oboe.h"

#include "opensles/StreamBuffered.h"
#include "common/AudioClock.h"

namespace oboe {

/*
 * Stream with a FifoBuffer
 */
StreamBuffered::StreamBuffered(const StreamBuilder &builder)
        : Stream(builder)
        , mFifoBuffer(NULL)
        , mInternalCallback(NULL)
{
}

Result StreamBuffered::open() {

    Result result = Stream::open();
    if (result != Result::OK) {
        return result;
    }

    // If the caller does not provide a callback use our own internal
    // callback that reads data from the FIFO.
    if (getCallback() == NULL) {
        LOGD("StreamBuffered(): new FifoBuffer");
        mFifoBuffer = new FifoBuffer(getBytesPerFrame(), 1024); // TODO size?
        // Create a callback that reads from the FIFO
        mInternalCallback = new AudioStreamBufferedCallback(this);
        mStreamCallback = mInternalCallback;
        LOGD("StreamBuffered(): mInternalCallback = %p", mInternalCallback);
    }
    return Result::OK;
}

StreamBuffered::~StreamBuffered() {
    delete mInternalCallback;
}

// TODO: This method should return a tuple of Result,int32_t where the 2nd return param is the frames written
int32_t StreamBuffered::write(const void *buffer,
                              int32_t numFrames,
                              int64_t timeoutNanoseconds)
{
    int32_t result = 0;
    uint8_t *source = (uint8_t *)buffer;
    int32_t framesLeft = numFrames;
    while(framesLeft > 0 && result >= 0) {
        result = mFifoBuffer->write(source, numFrames);
        LOGD("StreamBuffered::writeNow(): wrote %d/%d frames", result, numFrames);
        if (result > 0) {
            source += mFifoBuffer->convertFramesToBytes(result);
            incrementFramesWritten(result);
            framesLeft -= result;
        }
        if (framesLeft > 0 && result >= 0) {
            int64_t wakeTimeNanos =  mFifoBuffer->getNextReadTime(getSampleRate());
            // TODO use timeoutNanoseconds
            AudioClock::sleepUntilNanoTime(wakeTimeNanos);
        }
    }
    return result;
}

Result StreamBuffered::setBufferSizeInFrames(int32_t requestedFrames)
{
    if (mFifoBuffer != nullptr) {
        if (requestedFrames > mFifoBuffer->getBufferCapacityInFrames()) {
            requestedFrames = mFifoBuffer->getBufferCapacityInFrames();
        }
        mFifoBuffer->setThresholdFrames(requestedFrames);
        return Result::OK;
    } else {
        return Result::ErrorUnimplemented;
    }
}


int32_t StreamBuffered::getBufferSizeInFrames() const {
    if (mFifoBuffer != nullptr) {
        return mFifoBuffer->getThresholdFrames();
    } else {
        return Stream::getBufferSizeInFrames();
    }
}

int32_t StreamBuffered::getBufferCapacityInFrames() const {
    if (mFifoBuffer != nullptr) {
        return mFifoBuffer->getBufferCapacityInFrames(); // Maybe set mBufferCapacity in constructor
    } else {
        return Stream::getBufferCapacityInFrames();
    }
}

} // namespace oboe