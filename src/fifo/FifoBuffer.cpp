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

#include <stdint.h>
#include <time.h>
#include <memory.h>
#include <assert.h>

#include "common/OboeDebug.h"
#include "fifo/FifoControllerBase.h"
#include "fifo/FifoController.h"
#include "fifo/FifoControllerIndirect.h"
#include "fifo/FifoBuffer.h"
#include "common/AudioClock.h"

namespace oboe {

FifoBuffer::FifoBuffer(uint32_t bytesPerFrame, uint32_t capacityInFrames)
        : mFrameCapacity(capacityInFrames)
        , mBytesPerFrame(bytesPerFrame)
        , mStorage(NULL)
        , mFramesReadCount(0)
        , mFramesUnderrunCount(0)
        , mUnderrunCount(0)
{
    assert(bytesPerFrame > 0);
    assert(capacityInFrames > 0);
    mFifo = new FifoController(capacityInFrames, capacityInFrames);
    // allocate buffer
    int32_t bytesPerBuffer = bytesPerFrame * capacityInFrames;
    mStorage = new uint8_t[bytesPerBuffer];
    mStorageOwned = true;
    LOGD("FifoProcessor: capacityInFrames = %d, bytesPerFrame = %d",
         capacityInFrames, bytesPerFrame);
}

FifoBuffer::FifoBuffer( uint32_t   bytesPerFrame,
                        uint32_t   capacityInFrames,
                        int64_t *  readIndexAddress,
                        int64_t *  writeIndexAddress,
                        uint8_t *  dataStorageAddress
                        )
        : mFrameCapacity(capacityInFrames)
        , mBytesPerFrame(bytesPerFrame)
        , mStorage(dataStorageAddress)
        , mFramesReadCount(0)
        , mFramesUnderrunCount(0)
        , mUnderrunCount(0)
{
    assert(bytesPerFrame > 0);
    assert(capacityInFrames > 0);
    mFifo = new FifoControllerIndirect(capacityInFrames,
                                       capacityInFrames,
                                       readIndexAddress,
                                       writeIndexAddress);
    mStorage = dataStorageAddress;
    mStorageOwned = false;
    LOGD("FifoProcessor: capacityInFrames = %d, bytesPerFrame = %d",
         capacityInFrames, bytesPerFrame);
}

FifoBuffer::~FifoBuffer() {
    if (mStorageOwned) {
        delete[] mStorage;
    }
    delete mFifo;
}


int32_t FifoBuffer::convertFramesToBytes(int32_t frames) {
    return frames * mBytesPerFrame;
}

int32_t FifoBuffer::read(void *buffer, int32_t numFrames) {
    int32_t framesAvailable = mFifo->getFullFramesAvailable();
    int32_t framesToRead = numFrames;
    // Is there enough data in the FIFO
    if (framesToRead > framesAvailable) {
        framesToRead = framesAvailable;
    }
    if (framesToRead <= 0) {
        return 0;
    }

    uint32_t readIndex = mFifo->getReadIndex();
    uint8_t *destination = reinterpret_cast<uint8_t *>(buffer);
    uint8_t *source = &mStorage[convertFramesToBytes(readIndex)];
    if ((readIndex + framesToRead) > mFrameCapacity) {
        // read in two parts, first part here is at the end of the mStorage buffer
        uint32_t frames1 = mFrameCapacity - readIndex;
        int32_t numBytes = convertFramesToBytes(frames1);
        if (numBytes < 0) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        memcpy(destination, source, static_cast<size_t>(numBytes));
        destination += numBytes;
        // read second part, which is at the beginning of mStorage
        source = &mStorage[0];
        int frames2 = framesToRead - frames1;
        numBytes = convertFramesToBytes(frames2);
        if (numBytes < 0) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        memcpy(destination, source, static_cast<size_t>(numBytes));
    } else {
        // just read in one shot
        int32_t numBytes = convertFramesToBytes(framesToRead);
        if (numBytes < 0) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        memcpy(destination, source, static_cast<size_t>(numBytes));
    }
    mFifo->advanceReadIndex(framesToRead);

    return framesToRead;
}

int32_t FifoBuffer::write(const void *buffer, int32_t framesToWrite) {
    int32_t framesAvailable = mFifo->getEmptyFramesAvailable();
    if (framesToWrite > framesAvailable) {
        framesToWrite = framesAvailable;
    }
    if (framesToWrite <= 0) {
        return 0;
    }

    uint32_t writeIndex = mFifo->getWriteIndex();
    int byteIndex = convertFramesToBytes(writeIndex);
    const uint8_t *source = reinterpret_cast<const uint8_t *>(buffer);
    uint8_t *destination = &mStorage[byteIndex];
    if ((writeIndex + framesToWrite) > mFrameCapacity) {
        // write in two parts, first part here
        int frames1 = mFrameCapacity - writeIndex;
        int32_t numBytes = convertFramesToBytes(frames1);
        if (numBytes < 0) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        memcpy(destination, source, static_cast<size_t>(numBytes));
        // read second part
        source += convertFramesToBytes(frames1);
        destination = &mStorage[0];
        int framesLeft = framesToWrite - frames1;
        numBytes = convertFramesToBytes(framesLeft);
        if (numBytes < 0) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        memcpy(destination, source, static_cast<size_t>(numBytes));
    } else {
        // just write in one shot
        int32_t numBytes = convertFramesToBytes(framesToWrite);
        if (numBytes < 0) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        memcpy(destination, source, static_cast<size_t>(numBytes));
    }
    mFifo->advanceWriteIndex(framesToWrite);

    return framesToWrite;
}

int32_t FifoBuffer::readNow(void *buffer, int32_t numFrames) {
    int32_t framesLeft = numFrames;
    int32_t framesRead = read(buffer, numFrames);
    framesLeft -= framesRead;
    mFramesReadCount += framesRead;
    mFramesUnderrunCount += framesLeft;
    // Zero out any samples we could not set.
    if (framesLeft > 0) {
        mUnderrunCount++;
        uint8_t *destination = reinterpret_cast<uint8_t *>(buffer);
        destination += convertFramesToBytes(framesRead); // point to first byte not set
        int32_t bytesToZero = convertFramesToBytes(framesLeft);
        memset(destination, 0, static_cast<size_t>(bytesToZero));
    }

    return framesRead;
}

uint32_t FifoBuffer::getThresholdFrames() const {
    return mFifo->getThreshold();
}

uint32_t FifoBuffer::getBufferCapacityInFrames() const {
    return mFifo->getFrameCapacity();
}

void FifoBuffer::setThresholdFrames(uint32_t threshold) {
    mFifo->setThreshold(threshold);
}

} // namespace oboe