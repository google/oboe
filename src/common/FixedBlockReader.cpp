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

#include "FixedBlockReader.h"


FixedBlockReader::FixedBlockReader(FixedBlockProcessor &fixedBlockProcessor)
        : FixedBlockAdapter(fixedBlockProcessor) {
    mPosition = mSize;
}

int32_t FixedBlockReader::open(int32_t bytesPerFixedBlock) {
    int32_t result = FixedBlockAdapter::open(bytesPerFixedBlock);
    mPosition = mSize; // Indicate no data in storage.
    return result;
}

int32_t FixedBlockReader::readFromStorage(uint8_t *buffer, int32_t numBytes) {
    int32_t bytesToRead = numBytes;
    int32_t dataAvailable = mSize - mPosition;
    if (bytesToRead > dataAvailable) {
        bytesToRead = dataAvailable;
    }
    memcpy(buffer, mStorage + mPosition, bytesToRead);
    mPosition += bytesToRead;
    return bytesToRead;
}

int32_t FixedBlockReader::processVariableBlock(uint8_t *buffer, int32_t numBytes) {
    int32_t result = 0;
    int32_t bytesLeft = numBytes;
    while(bytesLeft > 0 && result == 0) {
        if (mPosition < mSize) {
            // Use up bytes currently in storage.
            int32_t bytesRead = readFromStorage(buffer, bytesLeft);
            buffer += bytesRead;
            bytesLeft -= bytesRead;
        } else if (bytesLeft >= mSize) {
            // Read through if enough for a complete block.
            result = mFixedBlockProcessor.onProcessFixedBlock(buffer, mSize);
            buffer += mSize;
            bytesLeft -= mSize;
        } else {
            // Just need a partial block so we have to use storage.
            result = mFixedBlockProcessor.onProcessFixedBlock(mStorage, mSize);
            mPosition = 0;
        }
    }
    return result;
}
