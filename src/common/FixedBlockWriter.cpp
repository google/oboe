/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include <memory.h>

#include "FixedBlockAdapter.h"
#include "FixedBlockWriter.h"

FixedBlockWriter::FixedBlockWriter(FixedBlockProcessor &fixedBlockProcessor)
        : FixedBlockAdapter(fixedBlockProcessor) {}


int32_t FixedBlockWriter::writeToStorage(uint8_t *buffer, int32_t numBytes) {
    int32_t bytesToStore = numBytes;
    int32_t roomAvailable = mSize - mPosition;
    if (bytesToStore > roomAvailable) {
        bytesToStore = roomAvailable;
    }
    memcpy(mStorage + mPosition, buffer, bytesToStore);
    mPosition += bytesToStore;
    return bytesToStore;
}

int32_t FixedBlockWriter::processVariableBlock(uint8_t *buffer, int32_t numBytes) {
    int32_t result = 0;
    int32_t bytesLeft = numBytes;

    // If we already have data in storage then add to it.
    if (mPosition > 0) {
        int32_t bytesWritten = writeToStorage(buffer, bytesLeft);
        buffer += bytesWritten;
        bytesLeft -= bytesWritten;
        // If storage full then flush it out
        if (mPosition == mSize) {
            result = mFixedBlockProcessor.onProcessFixedBlock(mStorage, mSize);
            mPosition = 0;
        }
    }

    // Write through if enough for a complete block.
    while(bytesLeft > mSize && result == 0) {
        result = mFixedBlockProcessor.onProcessFixedBlock(buffer, mSize);
        buffer += mSize;
        bytesLeft -= mSize;
    }

    // Save any remaining partial block for next time.
    if (bytesLeft > 0) {
        writeToStorage(buffer, bytesLeft);
    }

    return result;
}
