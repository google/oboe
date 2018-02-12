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

#include "opensles/OboeStreamBuffered.h"
#include "common/AudioClock.h"

/*
 * OboeStream with a FifoBuffer
 */
OboeStreamBuffered::OboeStreamBuffered(const OboeStreamBuilder &builder)
        : OboeStream(builder)
        , mFifoBuffer(NULL)
        , mInternalCallback(NULL)
{
}

oboe_result_t OboeStreamBuffered::open() {

    oboe_result_t result = OboeStream::open();
    if (result < 0) {
        return result;
    }

    // If the caller does not provide a callback use our own internal
    // callback that reads data from the FIFO.
    if (getCallback() == NULL) {
        LOGD("OboeStreamBuffered(): new FifoBuffer");
        mFifoBuffer = new FifoBuffer(getBytesPerFrame(), 1024); // TODO size?
        // Create a callback that reads from the FIFO
        mInternalCallback = new AudioStreamBufferedCallback(this);
        mStreamCallback = mInternalCallback;
        LOGD("OboeStreamBuffered(): mInternalCallback = %p", mInternalCallback);
    }
    return OBOE_OK;
}

OboeStreamBuffered::~OboeStreamBuffered() {
    delete mInternalCallback;
}

oboe_result_t OboeStreamBuffered::write(const void *buffer,
                                         int32_t numFrames,
                                         int64_t timeoutNanoseconds)
{
    oboe_result_t result = OBOE_OK;
    uint8_t *source = (uint8_t *)buffer;
    int32_t framesLeft = numFrames;
    while(framesLeft > 0 && result >= 0) {
        oboe_result_t result = mFifoBuffer->write(source, numFrames);
        LOGD("OboeStreamBuffered::writeNow(): wrote %d/%d frames", result, numFrames);
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

oboe_result_t OboeStreamBuffered::setBufferSizeInFrames(int32_t requestedFrames)
{
    if (mFifoBuffer != nullptr) {
        if (requestedFrames > mFifoBuffer->getBufferCapacityInFrames()) {
            requestedFrames = mFifoBuffer->getBufferCapacityInFrames();
        }
        mFifoBuffer->setThresholdFrames(requestedFrames);
        return OBOE_OK;
    } else {
        return OBOE_ERROR_UNIMPLEMENTED;
    }
}


int32_t OboeStreamBuffered::getBufferSizeInFrames() const {
    if (mFifoBuffer != nullptr) {
        return mFifoBuffer->getThresholdFrames();
    } else {
        return OboeStream::getBufferSizeInFrames();
    }
}

int32_t OboeStreamBuffered::getBufferCapacityInFrames() const {
    if (mFifoBuffer != nullptr) {
        return mFifoBuffer->getBufferCapacityInFrames(); // Maybe set mBufferCapacity in constructor
    } else {
        return OboeStream::getBufferCapacityInFrames();
    }
}
