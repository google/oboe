/*
 * Copyright 2019 The Android Open Source Project
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
#include <algorithm>
#include <string.h>

#include "MemInputStream.h"

namespace wavlib {

long MemInputStream::read(void *buff, long numBytes) {
    long numAvail = mBufferLen - mPos;
    numBytes = std::min(numBytes, numAvail);

    memcpy(buff, mBuffer + mPos, numBytes);
    mPos += numBytes;
    return numBytes;
}

long MemInputStream::peek(void *buff, long numBytes) {
    long numAvail = mBufferLen - mPos;
    numBytes = std::min(numBytes, numAvail);
    memcpy(buff, mBuffer + mPos, numBytes);
    return numBytes;
}

void MemInputStream::advance(long numBytes) {
    if (numBytes > 0) {
        long numAvail = mBufferLen - mPos;
        mPos += std::min(numAvail, numBytes);
    }
}

long MemInputStream::getPos() {
    return mPos;
}

void MemInputStream::setPos(long pos) {
    if (pos > 0) {
        if (pos < mBufferLen) {
            mPos = pos;
        } else {
            mPos = mBufferLen - 1;
        }
    }
}

} // namespace wavelib
