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

#ifndef OBOE_FIFOPROCESSOR_H
#define OBOE_FIFOPROCESSOR_H

#include <unistd.h>
#include <sys/types.h>

#include "common/OboeDebug.h"
#include "FifoControllerBase.h"
#include "oboe/Definitions.h"

namespace oboe {

class FifoBuffer {
public:
    FifoBuffer(uint32_t bytesPerFrame, uint32_t capacityInFrames);

    FifoBuffer(uint32_t   bytesPerFrame,
               uint32_t   capacityInFrames,
               int64_t * readCounterAddress,
               int64_t * writeCounterAddress,
               uint8_t * dataStorageAddress);

    ~FifoBuffer();

    int32_t convertFramesToBytes(int32_t frames);

    int32_t read(void *destination, int32_t framesToRead);

    int32_t write(const void *source, int32_t framesToWrite);

    uint32_t getThresholdFrames() const;

    void setThresholdFrames(uint32_t threshold);

    uint32_t getBufferCapacityInFrames() const;

    int32_t readNow(void *buffer, int32_t numFrames);

    uint32_t getUnderrunCount() const { return mUnderrunCount; }

    FifoControllerBase *getFifoControllerBase() { return mFifo; }

    uint32_t getBytesPerFrame() const {
        return mBytesPerFrame;
    }

    uint64_t getReadCounter() const {
        return mFifo->getReadCounter();
    }

    void setReadCounter(uint64_t n) {
        mFifo->setReadCounter(n);
//        LOGD("FifoBuffer: setReadCounter(%d)", (int) n);
    }

    uint64_t getWriteCounter() {
        return mFifo->getWriteCounter();
    }
    void setWriteCounter(uint64_t n) {
        mFifo->setWriteCounter(n);
//        LOGD("FifoBuffer: setWriteCounter(%d)", (int) n);
    }

private:
    uint32_t mFrameCapacity;
    uint32_t mBytesPerFrame;
    uint8_t* mStorage;
    bool     mStorageOwned; // did this object allocate the storage?
    FifoControllerBase *mFifo;
    uint64_t mFramesReadCount;
    uint64_t mFramesUnderrunCount;
    uint32_t mUnderrunCount; // need? just use frames
};

} // namespace oboe

#endif //OBOE_FIFOPROCESSOR_H
