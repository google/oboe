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
#ifndef _IO_WAV_WAVCHUNKHEADER_H_
#define _IO_WAV_WAVCHUNKHEADER_H_

#include "WavTypes.h"

namespace wavlib {

class InputStream;

class WavChunkHeader {
public:
    static const RiffID RIFFID_DATA;

    RiffID mChunkId;
    RiffInt32 mChunkSize;

    WavChunkHeader() : mChunkId(0), mChunkSize(0) {}

    WavChunkHeader(RiffID chunkId) : mChunkId(chunkId), mChunkSize(0) {}

    virtual void readHeader(InputStream *stream);

//	virtual void readBody(InputStream* stream);

//	public void skipBody(InputStream stream) throws IOException {
//		stream.read(readBuff_);
//		mChunkSize = ByteUtils.leBytesToInt(readBuff_);
//		stream.skip(mChunkSize);
//	}
//
//	public void skipBody(RandomAccessFile file) throws IOException {
//		file.read(readBuff_);
//		mChunkSize = ByteUtils.leBytesToInt(readBuff_);
//		file.skipBytes(mChunkSize);
//	}
//
//	/**
//	 * @param file
//	 * @return The file position of the chunk size field (for when the size of the chunk isn't
//	 * known a-priory).
//	 * @throws IOException
//	 */
//	public long write(RandomAccessFile file) throws IOException {
//		file.write(mChunkId);
//		long sizeFieldFilePos = file.getFilePointer();
//		file.write(ByteUtils.intToleBytes(mChunkSize));
//		return sizeFieldFilePos;
    private:
//	byte readBuff_[4];
    };

} // namespace wavlib

#endif // _IO_WAV_WAVCHUNKHEADER_H_
