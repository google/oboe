/*
 * Copyright 2025 The Android Open Source Project
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

#include "SinkMemoryDirect.h"
#include "common/OboeDebug.h"

SinkMemoryDirect::SinkMemoryDirect(int channelCount, int bytesPerFrame) :
        oboe::flowgraph::FlowGraphSink(channelCount), mBytesPerFrame(bytesPerFrame) {
}

void SinkMemoryDirect::setupMemoryBuffer(std::unique_ptr<uint8_t[]>& buffer, int length) {
    mBuffer = std::make_unique<uint8_t[]>(length);
    memcpy(mBuffer.get(), buffer.get(), length);
    mBufferLength = length;
    mCurPosition = 0;
}

void SinkMemoryDirect::reset() {
    oboe::flowgraph::FlowGraphNode::reset();
    mCurPosition = 0;
}

int32_t SinkMemoryDirect::read(void *data, int32_t numFrames) {
    auto uint8Data = static_cast<uint8_t*>(data);
    int bytesLeft = numFrames * mBytesPerFrame;
    while (bytesLeft > 0) {
        int bytesToCopy = std::min(bytesLeft, mBufferLength - mCurPosition);
        memcpy(uint8Data, mBuffer.get() + mCurPosition, bytesToCopy);
        mCurPosition += bytesToCopy;
        if (mCurPosition >= mBufferLength) {
            mCurPosition = 0;
        }
        bytesLeft -= bytesToCopy;
        uint8Data += bytesToCopy;
    }
    return numFrames;
}
