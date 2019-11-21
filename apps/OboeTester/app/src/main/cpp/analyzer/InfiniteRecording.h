/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef OBOETESTER_INFINITE_RECORDING_H
#define OBOETESTER_INFINITE_RECORDING_H

#include <memory>
#include <unistd.h>

/**
 * Record forever. Keep last data.
 */
class InfiniteRecording {
public:
    InfiniteRecording(size_t maxSamples)
        : mMaxSamples(maxSamples) {
        mData = std::make_unique<float[]>(mMaxSamples);
    }

    int32_t readFrom(float *buffer, size_t position, size_t count) {
        size_t maxPosition = mWritten.load();
        position = std::min(position, maxPosition);
        size_t numToRead = std::min(count, mMaxSamples);
        numToRead = std::min(numToRead, maxPosition - position);
        if (numToRead == 0) return 0;
        // We may need to read in two parts if it wraps.
        size_t offset = position % mMaxSamples;
        size_t firstReadSize = std::min(numToRead, mMaxSamples - offset); // till end
        std::copy(&mData[offset], &mData[offset + firstReadSize], buffer);
        if (firstReadSize < numToRead) {
            // Second read needed.
            std::copy(&mData[0], &mData[numToRead - firstReadSize], &buffer[firstReadSize]);
        }
        return numToRead;
    }

    void write(float sample) {
        size_t position = mWritten.load();
        size_t offset = position % mMaxSamples;
        mData[offset] = sample;
        mWritten++;
    }

    int64_t getTotalWritten() {
        return mWritten.load();
    }

private:
    std::unique_ptr<float[]> mData;
    std::atomic<size_t> mWritten{0};
    const size_t mMaxSamples;
};
#endif //OBOETESTER_INFINITE_RECORDING_H
