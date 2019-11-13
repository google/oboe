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
#ifndef _IO_STREAM_INPUTSTREAM_H_
#define _IO_STREAM_INPUTSTREAM_H_

namespace wavlib {

class InputStream {
public:
    InputStream() {}

    virtual long read(void *buff, long numBytes) = 0;

    virtual long peek(void *buff, long numBytes) = 0;

    virtual void advance(long numBytes) = 0;

    virtual long getPos() = 0;

    virtual void setPos(long pos) = 0;
};

} // namespace wavlib

#endif // _IO_STREAM_INPUTSTREAM_H_
