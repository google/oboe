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

#include "FifoControllerBase.h"

#include <assert.h>
#include <sys/types.h>
#include "FifoControllerBase.h"

#include "common/OboeDebug.h"

namespace oboe {

FifoControllerBase::FifoControllerBase(uint32_t totalFrames, uint32_t threshold)
        : mTotalFrames(totalFrames)
        , mThreshold(threshold)
{
}

uint32_t FifoControllerBase::getFullFramesAvailable() const {
    int64_t framesAvailable = getWriteCounter() - getReadCounter();
    return (framesAvailable < 0) ? 0 : static_cast<uint32_t>(framesAvailable);
}

uint32_t FifoControllerBase::getReadIndex() const {
    return static_cast<uint32_t>(getReadCounter() % mTotalFrames);
}

void FifoControllerBase::advanceReadIndex(uint32_t numFrames) {
    incrementReadCounter(numFrames);
}

uint32_t FifoControllerBase::getEmptyFramesAvailable() const {
    uint32_t fullFramesAvailable = getFullFramesAvailable();
    return (fullFramesAvailable > mThreshold) ? 0 : (mThreshold - fullFramesAvailable);
}

uint32_t FifoControllerBase::getWriteIndex() const {
    return static_cast<uint32_t>(getWriteCounter() % mTotalFrames); // % works with non-power of two sizes
}

void FifoControllerBase::advanceWriteIndex(uint32_t numFrames) {
    incrementWriteCounter(numFrames);
}

void FifoControllerBase::setThreshold(uint32_t threshold) {
    assert(threshold < mTotalFrames);
    mThreshold = threshold;
}

} // namespace oboe
