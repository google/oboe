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
#include <unistd.h>

#include "FileInputStream.h"

namespace wavlib {

long FileInputStream::read(void *buff, long numBytes) {
    return ::read(mFH, buff, numBytes);
}

long FileInputStream::peek(void *buff, long numBytes) {
    long numRead = ::read(mFH, buff, numBytes);
    ::lseek(mFH, -numBytes, SEEK_CUR);
    return numRead;
}

void FileInputStream::advance(long numBytes) {
    ::lseek(mFH, numBytes, SEEK_CUR);
}

long FileInputStream::getPos() {
    return ::lseek(mFH, 0L, SEEK_CUR);
}

void FileInputStream::setPos(long pos) {
    ::lseek(mFH, pos, SEEK_SET);
}

} /* namespace wavlib */
