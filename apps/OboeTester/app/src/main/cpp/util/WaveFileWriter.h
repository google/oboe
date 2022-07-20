/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Based on the WaveFileWriter in Java from the open source JSyn library by Phil Burk
// https://github.com/philburk/jsyn/blob/master/src/main/java/com/jsyn/util/WaveFileWriter.java

#ifndef UTIL_WAVE_FILE_WRITER
#define UTIL_WAVE_FILE_WRITER

#include <cassert>
#include <stdio.h>

class WaveFileOutputStream {
public:
    virtual ~WaveFileOutputStream() = default;
    virtual void write(uint8_t b) = 0;
};

/**
 * Write audio data to a WAV file.
 *
 * <pre>
 * <code>
 * WaveFileWriter writer = new WaveFileWriter(waveFileOutputStream);
 * writer.setFrameRate(48000);
 * writer.setBitsPerSample(24);
 * writer.write(floatArray, 0, numSamples);
 * writer.close();
 * </code>
 * </pre>
 *
 */
class WaveFileWriter {
public:

    /**
     * Create an object that will write a WAV file image to the specified stream.
     *
     * @param outputStream stream to receive the bytes
     * @throws FileNotFoundException
     */
    WaveFileWriter(WaveFileOutputStream *outputStream) {
        mOutputStream = outputStream;
    }

    /**
     * @param frameRate default is 44100
     */
    void setFrameRate(int32_t frameRate) {
        mFrameRate = frameRate;
    }

    int32_t getFrameRate() const {
        return mFrameRate;
    }

    /**
     * For stereo, set this to 2. Default is mono = 1.
     * Also known as ChannelCount
     */
    void setSamplesPerFrame(int32_t samplesPerFrame) {
        mSamplesPerFrame = samplesPerFrame;
    }

    int32_t getSamplesPerFrame() const {
        return mSamplesPerFrame;
    }

    /** Only 16 or 24 bit samples supported at the moment. Default is 16. */
    void setBitsPerSample(int32_t bits) {
        assert((bits == 16) || (bits == 24));
        bitsPerSample = bits;
    }

    int32_t getBitsPerSample() const {
        return bitsPerSample;
    }

    void close() {
    }

    /** Write single audio data value to the WAV file. */
    void write(float value);

    /**
     * Write a buffer to the WAV file.
     */
    void write(float *buffer, int32_t startSample, int32_t numSamples);

private:
    /**
     * Write a 32 bit integer to the stream in Little Endian format.
     */
    void writeIntLittle(int32_t n);

    /**
     * Write a 16 bit integer to the stream in Little Endian format.
     */
    void writeShortLittle(int16_t n);

    /**
     * Write an 'fmt ' chunk to the WAV file containing the given information.
     */
    void writeFormatChunk();

    /**
     * Write a 'data' chunk header to the WAV file. This should be followed by call to
     * writeShortLittle() to write the data to the chunk.
     */
    void writeDataChunkHeader();

    /**
     * Write a simple WAV header for PCM data.
     */
    void writeHeader();

    // Write lower 8 bits. Upper bits ignored.
    void writeByte(uint8_t b);

    void writePCM24(float value);

    void writePCM16(float value);

    /**
     * Write a 'RIFF' file header and a 'WAVE' ID to the WAV file.
     */
    void writeRiffHeader();

    static constexpr int WAVE_FORMAT_PCM = 1;
    WaveFileOutputStream *mOutputStream = nullptr;
    int32_t mFrameRate = 48000;
    int32_t mSamplesPerFrame = 1;
    int32_t bitsPerSample = 16;
    int32_t bytesWritten = 0;
    bool headerWritten = false;
    static constexpr int32_t PCM24_MIN = -(1 << 23);
    static constexpr int32_t PCM24_MAX = (1 << 23) - 1;

};

#endif /* UTIL_WAVE_FILE_WRITER */

