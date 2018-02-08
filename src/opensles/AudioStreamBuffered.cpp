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

#include "opensles/AudioStreamBuffered.h"
#include "common/AudioClock.h"

namespace oboe {

/*
 * AudioStream with a FifoBuffer
 */
AudioStreamBuffered::AudioStreamBuffered(const AudioStreamBuilder &builder)
        : AudioStream(builder) {
}
AudioStreamBuffered::~AudioStreamBuffered() {
    delete mFifoBuffer;
}

Result AudioStreamBuffered::finishOpen() {
    // If the caller does not provide a callback use our own internal
    // callback that reads data from the FIFO.
    if (getCallback() == nullptr) {
        LOGD("AudioStreamBuffered(): new FifoBuffer(bytesPerFrame=%d", getBytesPerFrame());
        // FIFO is configured with the same format and channels as the stream.
        mFifoBuffer = new FifoBuffer(getBytesPerFrame(), 1024); // TODO size?
        // Create a callback that reads from the FIFO
        mInternalCallback = std::unique_ptr<AudioStreamBufferedCallback>(new AudioStreamBufferedCallback(this));
        mStreamCallback = mInternalCallback.get();
        LOGD("AudioStreamBuffered(): mStreamCallback = %p", mStreamCallback);
    }
    return Result::OK;
}

// TODO: This method should return a tuple of Result,int32_t where the
// 2nd return param is the frames written. Maybe not!
int32_t AudioStreamBuffered::write(const void *buffer,
                                   int32_t numFrames,
                                   int64_t timeoutNanoseconds)
{
    int32_t result = 0;
    uint8_t *source = (uint8_t *)buffer;
    int32_t framesLeft = numFrames;
    while(framesLeft > 0 && result >= 0) {
        result = mFifoBuffer->write(source, framesLeft);
        LOGD("AudioStreamBuffered::%s(): wrote %d / %d frames to FIFO, [0] = %f",
             __func__, result, framesLeft, ((float *)source)[0]);
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

// Read from the FIFO that was written by the callback.
int32_t AudioStreamBuffered::read(void *buffer,
                                  int32_t numFrames,
                                  int64_t timeoutNanoseconds)
{
    static int readCount = 0;
    int32_t result = 0;
    uint8_t *destination = (uint8_t *)buffer;
    int32_t framesLeft = numFrames;
    while(framesLeft > 0 && result >= 0) {
        result = mFifoBuffer->read(destination, framesLeft);
        LOGD("AudioStreamBuffered::%s(): read %d/%d frames from FIFO, #%d",
             __func__, result, framesLeft, readCount);
        if (result > 0) {
            destination += mFifoBuffer->convertFramesToBytes(result);
            incrementFramesRead(result);
            framesLeft -= result;
            readCount++;
        }
        if (framesLeft > 0 && result >= 0) {
            // FIXME use proper timing model, borrow one from AAudio
            // TODO use timeoutNanoseconds
            AudioClock::sleepForNanos(4 * kNanosPerMillisecond);
        }
    }

    return result;
}

Result AudioStreamBuffered::setBufferSizeInFrames(int32_t requestedFrames)
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


int32_t AudioStreamBuffered::getBufferSizeInFrames() const {
    if (mFifoBuffer != nullptr) {
        return mFifoBuffer->getThresholdFrames();
    } else {
        return AudioStream::getBufferSizeInFrames();
    }
}

int32_t AudioStreamBuffered::getBufferCapacityInFrames() const {
    if (mFifoBuffer != nullptr) {
        return mFifoBuffer->getBufferCapacityInFrames(); // Maybe set mBufferCapacity in constructor
    } else {
        return AudioStream::getBufferCapacityInFrames();
    }
}

} // namespace oboe