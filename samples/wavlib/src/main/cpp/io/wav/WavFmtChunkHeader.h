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
#ifndef _IO_WAV_WAVFMTCHUNKHEADER_H_
#define _IO_WAV_WAVFMTCHUNKHEADER_H_

#include "WavChunkHeader.h"

class InputStream;

namespace wavlib {

class WavFmtChunkHeader : public WavChunkHeader {
public:
    static const RiffID RIFFID_FMT;

    // Microsoft format IDs
    static const short kFORMAT_PCM = 1;
    static const short kFORMAT_ADPCM = 2; // Microsoft ADPCM Format
    static const short kFORMAT_IEEE_FLOAT = 3; // samples from -1.0 -> 1.0

    RiffInt16 mFormatId;
    RiffInt16 mNumChannels;
    RiffInt32 mSampleRate;
    RiffInt32 mAveBytesPerSecond;
    RiffInt16 mBlockAlign;
    RiffInt16 mSampleSize;
    RiffInt16 mExtraBytes;

    WavFmtChunkHeader();

    WavFmtChunkHeader(RiffID tag);

    void normalize();

    void readHeader(InputStream *stream);

    void log();
};

} // namespace wavlib

#endif // _IO_WAV_WAVFMTCHUNKHEADER_H_
