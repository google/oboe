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

#ifndef NATIVEOBOE_FIFOCONTROLLERBASE_H
#define NATIVEOBOE_FIFOCONTROLLERBASE_H

#include <stdint.h>
#include <sys/types.h>

namespace oboe {

/**
 * Manage the read/write indices of a circular buffer.
 *
 * The caller is responsible for reading and writing the actual data.
 * Note that the span of available frames may not be contiguous. They
 * may wrap around from the end to the beginning of the buffer. In that
 * case the data must be read or written in at least two blocks of frames.
 *
 */

class FifoControllerBase {

public:
    /**
     * Constructor for FifoControllerBase
     * @param numFrames Size of the circular buffer in frames. Must be a power of 2.
     */
    FifoControllerBase(uint32_t totalFrames, uint32_t threshold);

    virtual ~FifoControllerBase();

    /**
     * This may be negative if an unthrottled reader has read beyond the available data.
     * @return number of valid frames available to read. Never read more than this.
     */
    int32_t getFullFramesAvailable();

    /**
     * The index in a circular buffer of the next frame to read.
     */
    uint32_t getReadIndex();

    /**
     * @param numFrames number of frames to advance the read index
     */
    void advanceReadIndex(int numFrames);

    /**
     * @return number of frames that can be written. Never write more than this.
     */
    int32_t getEmptyFramesAvailable();

    /**
     * The index in a circular buffer of the next frame to write.
     */
    uint32_t getWriteIndex();

    /**
     * @param numFrames number of frames to advance the write index
     */
    void advanceWriteIndex(uint32_t numFrames);

    void setThreshold(uint32_t threshold);

    uint32_t getThreshold() const { return mThreshold; }

    uint32_t getFrameCapacity() const { return mTotalFrames; }

    virtual uint64_t getReadCounter() = 0;
    virtual void setReadCounter(uint64_t n) = 0;
    virtual uint64_t getWriteCounter() = 0;
    virtual void setWriteCounter(uint64_t n) = 0;

private:
    uint32_t mTotalFrames;
    uint32_t mThreshold;
};

} // namespace oboe

#endif //NATIVEOBOE_FIFOCONTROLLERBASE_H
