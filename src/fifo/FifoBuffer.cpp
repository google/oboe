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


uint32_t FifoBuffer::convertFramesToBytes(uint32_t frames) {
    return frames * mBytesPerFrame;
}

int32_t FifoBuffer::read(void *buffer, int32_t numFrames) {
    int32_t framesAvailable = mFifo->getFullFramesAvailable();
    int32_t framesToRead = numFrames;
    // Is there enough data in the FIFO?
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
        // First, read frames until the end of the mStorage buffer.
        if(readIndex > mFrameCapacity) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        uint32_t framesUntilEnd = mFrameCapacity - readIndex;
        uint32_t numBytes = convertFramesToBytes(framesUntilEnd);
        memcpy(destination, source, static_cast<size_t>(numBytes));
        destination += numBytes;

        // Second, read remaining frames from the beginning of mStorage.
        if(framesUntilEnd > framesToRead) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        uint32_t framesFromBeginning = framesToRead - framesUntilEnd;
        numBytes = convertFramesToBytes(framesFromBeginning);
        source = &mStorage[0];
        memcpy(destination, source, static_cast<size_t>(numBytes));
    } else {
        // Contiguous read all frames.
        uint32_t numBytes = convertFramesToBytes(framesToRead);
        memcpy(destination, source, static_cast<size_t>(numBytes));
    }
    mFifo->advanceReadIndex(framesToRead);

    return framesToRead;
}

int32_t FifoBuffer::write(const void *buffer, int32_t framesToWrite) {
    int32_t framesAvailable = mFifo->getEmptyFramesAvailable();
    // Is there enough space in the FIFO?
    if (framesToWrite > framesAvailable) {
        framesToWrite = framesAvailable;
    }
    if (framesToWrite <= 0) {
        return 0;
    }

    uint32_t writeIndex = mFifo->getWriteIndex();
    uint32_t byteIndex = convertFramesToBytes(writeIndex);
    const uint8_t *source = reinterpret_cast<const uint8_t *>(buffer);
    uint8_t *destination = &mStorage[byteIndex];
    if ((writeIndex + framesToWrite) > mFrameCapacity) {
        // First, write frames until the end of the mStorage buffer.
        if(writeIndex > mFrameCapacity) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        uint32_t framesUntilEnd = mFrameCapacity - writeIndex;
        uint32_t numBytes = convertFramesToBytes(framesUntilEnd);
        memcpy(destination, source, static_cast<size_t>(numBytes));

        // Second, write remaining frames from the beginning of the storage.
        source += numBytes;
        destination = &mStorage[0];
        if (framesUntilEnd > framesToWrite) {
            return static_cast<int32_t>(Result::ErrorOutOfRange);
        }
        uint32_t framesLeft = framesToWrite - framesUntilEnd;
        numBytes = convertFramesToBytes(framesLeft);
        memcpy(destination, source, static_cast<size_t>(numBytes));
    } else {
        // Contiguously write all frames.
        int32_t numBytes = convertFramesToBytes(framesToWrite);
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
        uint32_t bytesToZero = convertFramesToBytes(framesLeft);
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