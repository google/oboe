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
#include <vector> // Include the vector header

#include <android/log.h>

#include "../stream/InputStream.h"

#include "AudioEncoding.h"
#include "WavRIFFChunkHeader.h"
#include "WavFmtChunkHeader.h"
#include "WavChunkHeader.h"
#include "WavStreamReader.h"

static const char *TAG = "WavStreamReader";

static constexpr int kConversionBufferFrames = 16;

namespace parselib {

    WavStreamReader::WavStreamReader(InputStream *stream) {
        mStream = stream;
        mWavChunk = nullptr;
        mFmtChunk = nullptr;
        mDataChunk = nullptr;
        mAudioDataStartPos = -1;
    }

    int WavStreamReader::getSampleEncoding() {
        if (mFmtChunk == nullptr) {
            return AudioEncoding::INVALID; // Handle case where fmt chunk is not parsed
        }

        if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
            switch (mFmtChunk->mSampleSize) {
                case 8:
                    return AudioEncoding::PCM_8;

                case 16:
                    return AudioEncoding::PCM_16;

                case 24:
                    return AudioEncoding::PCM_24;

                case 32:
                    return AudioEncoding::PCM_32;

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
            // Check if stream is valid and can peek
            if (mStream == nullptr) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Stream is null in parse()");
                break;
            }

            int numRead = mStream->peek(&tag, sizeof(tag));
            if (numRead <= 0) {
                break; // done or error
            }

            std::shared_ptr<WavChunkHeader> chunk = nullptr;
            if (tag == WavRIFFChunkHeader::RIFFID_RIFF) {
                // Create a new shared_ptr and assign to mWavChunk
                mWavChunk = std::make_shared<WavRIFFChunkHeader>(WavRIFFChunkHeader(tag));
                chunk = mWavChunk;
                mWavChunk->read(mStream);
            } else if (tag == WavFmtChunkHeader::RIFFID_FMT) {
                // Create a new shared_ptr and assign to mFmtChunk
                mFmtChunk = std::make_shared<WavFmtChunkHeader>(WavFmtChunkHeader(tag));
                chunk = mFmtChunk;
                mFmtChunk->read(mStream);
            } else if (tag == WavChunkHeader::RIFFID_DATA) {
                // Create a new shared_ptr and assign to mDataChunk
                mDataChunk = std::make_shared<WavChunkHeader>(WavChunkHeader(tag));
                chunk = mDataChunk;
                mDataChunk->read(mStream);
                // We are now positioned at the start of the audio data.
                mAudioDataStartPos = mStream->getPos();
                mStream->advance(mDataChunk->mChunkSize);
            } else {
                // Create a new shared_ptr for unknown chunks
                chunk = std::make_shared<WavChunkHeader>(WavChunkHeader(tag));
                chunk->read(mStream);
                mStream->advance(chunk->mChunkSize); // skip the body
            }

            mChunkMap[tag] = chunk;
        }

        // After parsing, set the stream position to the start of the audio data if data chunk found
        if (mDataChunk != nullptr && mAudioDataStartPos != -1) {
            mStream->setPos(mAudioDataStartPos);
        }
    }

// Data access
    void WavStreamReader::positionToAudio() {
        if (mDataChunk != nullptr && mAudioDataStartPos != -1) {
            mStream->setPos(mAudioDataStartPos);
        }
    }

/**
 * Read and convert samples in PCM8 format to float
 */
    int WavStreamReader::getDataFloat_PCM8(float *buff, int numFrames) {
        if (mFmtChunk == nullptr) {
            return ERR_INVALID_STATE;
        }
        int numChannels = mFmtChunk->mNumChannels;

        int buffOffset = 0;
        int totalFramesRead = 0;

        static constexpr int kSampleSize = sizeof(u_int8_t);
        static constexpr float kSampleFullScale = (float) 0x80;
        static constexpr float kInverseScale = 1.0f / kSampleFullScale;

        // Use std::vector for dynamic buffer size
        std::vector<u_int8_t> readBuff(kConversionBufferFrames * numChannels);
        int framesLeft = numFrames;
        while (framesLeft > 0) {
            int framesThisRead = std::min(framesLeft, kConversionBufferFrames);
            int numSamplesToRead = framesThisRead * kSampleSize * numChannels;

            // Ensure vector has enough capacity
            if (readBuff.size() < (size_t) numSamplesToRead) {
                readBuff.resize(numSamplesToRead);
            }

            int numBytesRead = mStream->read(readBuff.data(), numSamplesToRead);
            if (numBytesRead <= 0) {
                break; // done or error
            }

            int numFramesReadThisIter = numBytesRead / (kSampleSize * numChannels);
            totalFramesRead += numFramesReadThisIter;

            // Convert & Scale
            for (int offset = 0; offset < numFramesReadThisIter * numChannels; offset++) {
                // PCM8 is unsigned, so we need to make it signed before scaling/converting
                buff[buffOffset++] = ((float) readBuff[offset] - kSampleFullScale)
                                     * kInverseScale;
            }

            framesLeft -= numFramesReadThisIter;
            if (numFramesReadThisIter < framesThisRead) {
                break; // read less than requested, possibly end of stream
            }
        }

        return totalFramesRead;
    }

/**
 * Read and convert samples in PCM16 format to float
 */
    int WavStreamReader::getDataFloat_PCM16(float *buff, int numFrames) {
        if (mFmtChunk == nullptr) {
            return ERR_INVALID_STATE;
        }
        int numChannels = mFmtChunk->mNumChannels;

        int buffOffset = 0;
        int totalFramesRead = 0;

        static constexpr int kSampleSize = sizeof(int16_t);
        static constexpr float kSampleFullScale = (float) 0x8000;
        static constexpr float kInverseScale = 1.0f / kSampleFullScale;

        // Use std::vector for dynamic buffer size
        std::vector<int16_t> readBuff(kConversionBufferFrames * numChannels);
        int framesLeft = numFrames;
        while (framesLeft > 0) {
            int framesThisRead = std::min(framesLeft, kConversionBufferFrames);
            int numSamplesToRead = framesThisRead * kSampleSize * numChannels;

            // Ensure vector has enough capacity
            if (readBuff.size() < (size_t) numSamplesToRead) {
                readBuff.resize(numSamplesToRead);
            }

            int numBytesRead = mStream->read(readBuff.data(), numSamplesToRead);
            if (numBytesRead <= 0) {
                break; // done or error
            }

            int numFramesReadThisIter = numBytesRead / (kSampleSize * numChannels);
            totalFramesRead += numFramesReadThisIter;

            // Convert & Scale
            for (int offset = 0; offset < numFramesReadThisIter * numChannels; offset++) {
                buff[buffOffset++] = (float) readBuff[offset] * kInverseScale;
            }

            framesLeft -= numFramesReadThisIter;
            if (numFramesReadThisIter < framesThisRead) {
                break; // read less than requested, possibly end of stream
            }
        }

        return totalFramesRead;
    }

/**
 * Read and convert samples in PCM24 format to float
 */
    int WavStreamReader::getDataFloat_PCM24(float *buff, int numFrames) {
        if (mFmtChunk == nullptr) {
            return ERR_INVALID_STATE;
        }
        int numChannels = mFmtChunk->mNumChannels;
        int numSamples = numFrames * numChannels;

        static constexpr float kSampleFullScale = (float) 0x80000000;
        static constexpr float kInverseScale = 1.0f / kSampleFullScale;

        uint8_t buffer[3];
        for (int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
            if (mStream == nullptr) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Stream is null in getDataFloat_PCM24()");
                return 0; // or handle error appropriately
            }
            if (mStream->read(buffer, 3) < 3) {
                numSamples = sampleIndex; // Adjust numSamples to reflect actual samples read
                break; // no more data or error
            }
            // Correct 24-bit to 32-bit signed conversion
            // WAV is little-endian, so the bytes are in order: byte0, byte1, byte2
            // We need to shift them into a 32-bit integer, sign-extending
            int32_t sample = (int32_t) ((buffer[2] << 24) | (buffer[1] << 16) | (buffer[0] << 8));
            // The highest bit of the 24-bit sample is the sign bit.
            // If it's set (value 0x800000 for the 24-bit value), the 32-bit value should be negative.
            // The current shift (<< 8) puts the 24-bit value in the most significant bits
            // of a 32-bit integer. If the original 24-bit value was negative (highest bit set),
            // the resulting 32-bit value will also have its highest bit set due to the shift.
            // So, the cast to int32_t handles the sign extension correctly.

            buff[sampleIndex] = (float) sample * kInverseScale;
        }

        // Return the number of frames successfully read
        return numSamples / numChannels;
    }

/**
 * Read and convert samples in Float32 format to float
 */
    int WavStreamReader::getDataFloat_Float32(float *buff, int numFrames) {
        // Turns out that WAV Float32 is just Android floats
        if (mFmtChunk == nullptr) {
            return ERR_INVALID_STATE;
        }
        int numChannels = mFmtChunk->mNumChannels;

        if (mStream == nullptr) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Stream is null in getDataFloat_Float32()");
            return 0; // or handle error appropriately
        }

        int numBytesToRead = numFrames * sizeof(float) * numChannels;
        int numBytesRead = mStream->read(buff, numBytesToRead);

        if (numBytesRead <= 0) {
            return 0; // done or error
        }

        return numBytesRead / (sizeof(float) * numChannels);
    }

/**
 * Read and convert samples in PCM32 format to float
 */
    int WavStreamReader::getDataFloat_PCM32(float *buff, int numFrames) {
        if (mFmtChunk == nullptr) {
            return ERR_INVALID_STATE;
        }
        int numChannels = mFmtChunk->mNumChannels;

        int buffOffset = 0;
        int totalFramesRead = 0;

        static constexpr int kSampleSize = sizeof(int32_t);
        static constexpr float kSampleFullScale = (float) 0x80000000; // Use signed max value for scaling
        static constexpr float kInverseScale = 1.0f / kSampleFullScale;

        // Use std::vector for dynamic buffer size
        std::vector<int32_t> readBuff(kConversionBufferFrames * numChannels);
        int framesLeft = numFrames;
        while (framesLeft > 0) {
            int framesThisRead = std::min(framesLeft, kConversionBufferFrames);
            int numSamplesToRead = framesThisRead * kSampleSize * numChannels;

            // Ensure vector has enough capacity
            if (readBuff.size() < (size_t) numSamplesToRead) {
                readBuff.resize(numSamplesToRead);
            }

            int numBytesRead = mStream->read(readBuff.data(), numSamplesToRead);
            if (numBytesRead <= 0) {
                break; // done or error
            }

            int numFramesReadThisIter = numBytesRead / (kSampleSize * numChannels);
            totalFramesRead += numFramesReadThisIter;

            // convert & Scale
            for (int offset = 0; offset < numFramesReadThisIter * numChannels; offset++) {
                // Scale signed 32-bit integer to float
                buff[buffOffset++] = (float) readBuff[offset] * kInverseScale;
            }

            framesLeft -= numFramesReadThisIter;
            if (numFramesReadThisIter < framesThisRead) {
                break; // read less than requested, possibly end of stream
            }
        }

        return totalFramesRead;
    }

    int WavStreamReader::getDataFloat(float *buff, int numFrames) {
        // __android_log_print(ANDROID_LOG_INFO, TAG, "getData(%d)", numFrames);

        if (mDataChunk == nullptr || mFmtChunk == nullptr || mStream == nullptr) {
            __android_log_print(ANDROID_LOG_ERROR, TAG,
                                "getDataFloat() invalid state: mDataChunk=%p, mFmtChunk=%p, mStream=%p",
                                mDataChunk.get(), mFmtChunk.get(), mStream);
            return ERR_INVALID_STATE;
        }

        int numFramesRead = 0;
        switch (mFmtChunk->mSampleSize) {
            case 8:
                numFramesRead = getDataFloat_PCM8(buff, numFrames);
                break;

            case 16:
                numFramesRead = getDataFloat_PCM16(buff, numFrames);
                break;

            case 24:
                if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
                    numFramesRead = getDataFloat_PCM24(buff, numFrames);
                } else {
                    __android_log_print(ANDROID_LOG_INFO, TAG,
                                        "getDataFloat() invalid encoding:%d mSampleSize:%d for 24-bit",
                                        mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
                    return ERR_INVALID_FORMAT;
                }
                break;

            case 32:
                if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
                    numFramesRead = getDataFloat_PCM32(buff, numFrames);
                } else if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_IEEE_FLOAT) {
                    numFramesRead = getDataFloat_Float32(buff, numFrames);
                } else {
                    __android_log_print(ANDROID_LOG_INFO, TAG,
                                        "getDataFloat() invalid encoding:%d mSampleSize:%d for 32-bit",
                                        mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
                    return ERR_INVALID_FORMAT;
                }
                break;

            default:
                __android_log_print(ANDROID_LOG_INFO, TAG,
                                    "getDataFloat() invalid encoding:%d mSampleSize:%d - unsupported",
                                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
                return ERR_INVALID_FORMAT;
        }

        // Zero out any unread frames if the total frames read is less than requested
        if (numFramesRead < numFrames) {
            int numChannels = getNumChannels();
            if (numChannels > 0) { // Prevent division by zero if fmt chunk wasn't parsed
                memset(buff + (size_t) (numFramesRead * numChannels), 0,
                       (size_t) (numFrames - numFramesRead) * sizeof(buff[0]) * numChannels);
            }
        }

        return numFramesRead;
    }

} // namespace parselib