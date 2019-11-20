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
#ifndef _IO_STREAM_MEMINPUTSTREAM_H_
#define _IO_STREAM_MEMINPUTSTREAM_H_

#include "InputStream.h"

namespace wavlib {

class MemInputStream : public InputStream {
public:
    MemInputStream(unsigned char *buff, long len) : mBuffer(buff), mPos(0), mBufferLen(len) {}
    virtual ~MemInputStream() {}

    virtual long read(void *buff, long numBytes);

    virtual long peek(void *buff, long numBytes);

    virtual void advance(long numBytes);

    virtual long getPos();

    virtual void setPos(long pos);

private:
    unsigned char *mBuffer;
    long mPos;
    long mBufferLen;
};

} // namespace wavlib

#endif // _IO_STREAM_MEMINPUTSTREAM_H_
