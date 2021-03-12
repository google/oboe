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

#include <android/log.h>

#include "stream/InputStream.h"

#include "AudioEncoding.h"
#include "WavRIFFChunkHeader.h"
#include "WavFmtChunkHeader.h"
#include "WavChunkHeader.h"
#include "WavStreamReader.h"

static const char *TAG = "WavStreamReader";

namespace parselib {

WavStreamReader::WavStreamReader(InputStream *stream) {
    mStream = stream;

    mWavChunk = nullptr;
    mFmtChunk = nullptr;
    mDataChunk = nullptr;

    mAudioDataStartPos = -1;
}

int WavStreamReader::getSampleEncoding() {
    if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
        switch (mFmtChunk->mSampleSize) {
            case 8:
                return AudioEncoding::PCM_8;

            case 16:
                return AudioEncoding::PCM_16;

            case 24:
                // TODO - Support 24-bit WAV data
                return AudioEncoding::INVALID; // for now

            default:
                return AudioEncoding::INVALID;
        }
    } else if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_IEEE_FLOAT) {
        return AudioEncoding::PCM_IEEEFLOAT;
    }

    return AudioEncoding::INVALID;
}

void WavStreamReader::parse() {
    RiffID tag;

    while (true) {
        int numRead = mStream->peek(&tag, sizeof(tag));
        if (numRead <= 0) {
            break; // done
        }

//        char *tagStr = (char *) &tag;
//        __android_log_print(ANDROID_LOG_INFO, TAG, "[%c%c%c%c]",
//                            tagStr[0], tagStr[1], tagStr[2], tagStr[3]);

        std::shared_ptr<WavChunkHeader> chunk = nullptr;
        if (tag == WavRIFFChunkHeader::RIFFID_RIFF) {
            chunk = mWavChunk = std::shared_ptr<WavRIFFChunkHeader>(new WavRIFFChunkHeader(tag));
            mWavChunk->read(mStream);
        } else if (tag == WavFmtChunkHeader::RIFFID_FMT) {
            chunk = mFmtChunk = std::shared_ptr<WavFmtChunkHeader>(new WavFmtChunkHeader(tag));
            mFmtChunk->read(mStream);
        } else if (tag == WavChunkHeader::RIFFID_DATA) {
            chunk = mDataChunk = std::shared_ptr<WavChunkHeader>(new WavChunkHeader(tag));
            mDataChunk->read(mStream);
            // We are now positioned at the start of the audio data.
            mAudioDataStartPos = mStream->getPos();
            mStream->advance(mDataChunk->mChunkSize);
        } else {
            chunk = std::shared_ptr<WavChunkHeader>(new WavChunkHeader(tag));
            chunk->read(mStream);
            mStream->advance(chunk->mChunkSize); // skip the body
        }

        mChunkMap[tag] = chunk;
    }

    if (mDataChunk != 0) {
        mStream->setPos(mAudioDataStartPos);
    }
}

// Data access
void WavStreamReader::positionToAudio() {
    if (mDataChunk != 0) {
        mStream->setPos(mAudioDataStartPos);
    }
}

/**
 * Read and convert samples in PCM8 format to float
 */
int WavStreamReader::getDataFloat_PCM8(float *buff, int numFrames) {
    int numChans = mFmtChunk->mNumChannels;

    int buffOffset = 0;
    int totalFramesRead = 0;

    const static int SAMPLE_SIZE = sizeof(u_int8_t);
    const static float SAMPLE_FULLSCALE = (float)0x7F;
    
    u_int8_t *readBuff = new u_int8_t[128 * numChans];
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        int framesThisRead = std::min(framesLeft, 128);
        //__android_log_print(ANDROID_LOG_INFO, TAG, "read(%d)", framesThisRead);
        int numFramesRead =
                mStream->read(readBuff, framesThisRead *  SAMPLE_SIZE * numChans) /
                (SAMPLE_SIZE * numChans);
        totalFramesRead += numFramesRead;

        // Convert & Scale
        for (int offset = 0; offset < numFramesRead * numChans; offset++) {
            // PCM8 is unsigned, so we need to make it signed before scaling/converting
            buff[buffOffset++] = ((float) readBuff[offset] - SAMPLE_FULLSCALE)
                    / (float) SAMPLE_FULLSCALE;
        }

        if (numFramesRead < framesThisRead) {
            break; // none left
        }

        framesLeft -= framesThisRead;
    }
    delete[] readBuff;

    return totalFramesRead;
}

/**
 * Read and convert samples in PCM16 format to float
 */
int WavStreamReader::getDataFloat_PCM16(float *buff, int numFrames) {
    int numChans = mFmtChunk->mNumChannels;

    int buffOffset = 0;
    int totalFramesRead = 0;

    const static int SAMPLE_SIZE = sizeof(int16_t);
    const static float SAMPLE_FULLSCALE = (float) 0x7FFF;

    int16_t *readBuff = new int16_t[128 * numChans];
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        int framesThisRead = std::min(framesLeft, 128);
        //__android_log_print(ANDROID_LOG_INFO, TAG, "read(%d)", framesThisRead);
        int numFramesRead =
                mStream->read(readBuff, framesThisRead * SAMPLE_SIZE * numChans) /
                (SAMPLE_SIZE * numChans);
        totalFramesRead += numFramesRead;

        // Convert & Scale
        for (int offset = 0; offset < numFramesRead * numChans; offset++) {
            buff[buffOffset++] = (float) readBuff[offset] / SAMPLE_FULLSCALE;
        }

        if (numFramesRead < framesThisRead) {
            break; // none left
        }

        framesLeft -= framesThisRead;
    }
    delete[] readBuff;

    return totalFramesRead;
}

/**
 * Read and convert samples in PCM24 format to float
 */
int WavStreamReader::getDataFloat_PCM24(float *buff, int numFrames) {
    int numChans = mFmtChunk->mNumChannels;
    int numSamples = numFrames * numChans;

    const static float SAMPLE_FULLSCALE = (float) 0x7FFFFFFF;

    uint8_t buffer[3];
    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
        mStream->read(buffer, 3);
        int32_t sample = (buffer[0] << 8) | (buffer[1] << 16) | (buffer[2] << 24);
        buff[sampleIndex] = (float)sample / SAMPLE_FULLSCALE;
    }

    return numFrames;
}

/**
 * Read and convert samples in Float32 format to float
 */
int WavStreamReader::getDataFloat_Float32(float *buff, int numFrames) {
    // Turns out that WAV Float32 is just Android floats
    int numChans = mFmtChunk->mNumChannels;

    return mStream->read(buff, numFrames * sizeof(float) * numChans) /
           (sizeof(float) * numChans);
}

/**
 * Read and convert samples in PCM32 format to float
 */
int WavStreamReader::getDataFloat_PCM32(float *buff, int numFrames) {
    int numChans = mFmtChunk->mNumChannels;

    int buffOffset = 0;
    int totalFramesRead = 0;

    const static int SAMPLE_SIZE = sizeof(int32_t);
    const static float SAMPLE_FULLSCALE = (float) 0x7FFFFFFF;

    int32_t *readBuff = new int32_t[128 * numChans];
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        int framesThisRead = std::min(framesLeft, 128);
        //__android_log_print(ANDROID_LOG_INFO, TAG, "read(%d)", framesThisRead);
        int numFramesRead =
                mStream->read(readBuff, framesThisRead *  SAMPLE_SIZE* numChans) /
                    (SAMPLE_SIZE * numChans);
        totalFramesRead += numFramesRead;

        // convert & Scale
        for (int offset = 0; offset < numFramesRead * numChans; offset++) {
            buff[buffOffset++] = (float) readBuff[offset] / SAMPLE_FULLSCALE;
        }

        if (numFramesRead < framesThisRead) {
            break; // none left
        }

        framesLeft -= framesThisRead;
    }
    delete[] readBuff;

    return totalFramesRead;
}

int WavStreamReader::getDataFloat(float *buff, int numFrames) {
    // __android_log_print(ANDROID_LOG_INFO, TAG, "getData(%d)", numFrames);

    if (mDataChunk == nullptr || mFmtChunk == nullptr) {
        return 0;
    }

    // TODO - Manage other input formats
    switch (mFmtChunk->mSampleSize) {
        case 8:
            return getDataFloat_PCM8(buff, numFrames);

        case 16:
            return getDataFloat_PCM16(buff, numFrames);

        case 24:
            if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
                return getDataFloat_PCM24(buff, numFrames);
            } else {
                __android_log_print(ANDROID_LOG_INFO, TAG, "invalid encoding:%d mSampleSize:%d",
                                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
                return 0;
            }

        case 32:
            if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
                return getDataFloat_PCM32(buff, numFrames);
            } else if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_IEEE_FLOAT) {
                return getDataFloat_Float32(buff, numFrames);
            } else {
                __android_log_print(ANDROID_LOG_INFO, TAG, "invalid encoding:%d mSampleSize:%d",
                                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
                return 0;
            }

        default:
            __android_log_print(ANDROID_LOG_INFO, TAG, "invalid encoding:%d mSampleSize:%d",
                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
    }

    return 0;
}

} // namespace parselib
