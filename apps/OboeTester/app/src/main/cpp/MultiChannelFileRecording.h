/*
 * Copyright 2025 The Android Open Source Project
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

#ifndef NATIVEOBOE_MULTICHANNEL_FILE_RECORDING_H
#define NATIVEOBOE_MULTICHANNEL_FILE_RECORDING_H

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>

/**
 * @class MultiChannelFileRecording
 * @brief Stores multi-channel audio data in float format directly to a file
 * on disk.
 *
 * This class provides file-like operations, managing read and write positions
 * within the underlying file. Data is always appended during write operations.
 *
 * Note that this class is NOT thread-safe. Do not read and write from separate
 * threads without external synchronization.
 */
class MultiChannelFileRecording {
public:
    /**
     * @brief Constructs a new MultiChannelFileRecording object, opening or creating
     * a file for audio storage.
     * @param channelCount The number of audio channels (e.g., 1 for mono, 2 for stereo).
     * @param filename The path to the file where audio data will be stored.
     * @throws std::runtime_error if the file cannot be opened.
     */
    MultiChannelFileRecording(int32_t channelCount, const std::string& filename)
            : mChannelCount(channelCount)
            , mFilename(filename) {
        // Open the file in binary mode for both reading and writing.
        // std::ios::ate sets the initial position to the end of the file.
        // This is useful to determine the initial file size (mWriteCursorFrames).
        mFileStream.open(
                mFilename, std::ios::binary | std::ios::in | std::ios::out | std::ios::ate);

        if (!mFileStream.is_open()) {
            // Attempt to create the file if it doesn't exist and opening failed.
            // This is a common pattern when you need a file to exist for R/W.
            mFileStream.clear(); // Clear any error flags from previous open attempt
            // Create/truncate
            mFileStream.open(mFilename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            if (!mFileStream.is_open()) {
                throw std::runtime_error(
                        "MultiChannelFileRecording: Failed to open or create file: " + mFilename);
            }
        }

        // Get the current file size in bytes to initialize mWriteCursorFrames.
        // For std::ios::ate, tellp() returns the end position.
        int64_t fileSizeInBytes = mFileStream.tellp();
        mWriteCursorFrames = fileSizeInBytes / (mChannelCount * sizeof(float));

        // Rewind read cursor to the beginning of the file.
        mReadCursorFrames = 0;
    }

    /**
     * @brief Destroys the MultiChannelFileRecording object and closes the file.
     */
    ~MultiChannelFileRecording() {
        if (mFileStream.is_open()) {
            mFileStream.close();
        }
    }

    /**
     * @brief Resets the read cursor to the beginning of the file.
     */
    void rewind() {
        mReadCursorFrames = 0;
    }

    /**
     * @brief Clears the recording by truncating the file to zero size.
     * This effectively deletes all recorded audio data.
     * The read and write cursors are reset to zero.
     * @throws std::runtime_error if the file cannot be cleared.
     */
    void clear() {
        // Close the current stream.
        if (mFileStream.is_open()) {
            mFileStream.close();
        }

        // Remove the file from disk.
        if (std::remove(mFilename.c_str()) != 0) {
            throw std::runtime_error(
                    "MultiChannelFileRecording: Failed to remove file during clear: " + mFilename);
        }

        // Re-open the file in truncate mode to create an empty file.
        mFileStream.open(mFilename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!mFileStream.is_open()) {
            throw std::runtime_error(
                    "MultiChannelFileRecording: Failed to re-open file after clear: " + mFilename);
        }

        // Reset cursors as the file is now empty.
        mReadCursorFrames = 0;
        mWriteCursorFrames = 0;
    }

    /**
     * @brief Gets the number of channels in the recording.
     * @return The channel count.
     */
    int32_t getChannelCount() {
        return mChannelCount;
    }

    /**
     * @brief Gets the total number of frames currently stored in the file.
     * This represents the total length of the recording.
     * @return The number of frames currently in the file.
     */
    int64_t getSizeInFrames() {
        return mWriteCursorFrames;
    }

    /**
     * @brief Writes 'numFrames' from a 16-bit integer buffer into the recording file.
     * The 16-bit samples are converted to floats and stored. Data is appended
     * to the end of the file.
     * @param buffer A pointer to the source 16-bit audio data.
     * @param numFrames The number of frames to write.
     * @return The number of frames actually written (should be 'numFrames').
     * @throws std::runtime_error if the write operation fails.
     */
    int32_t write(int16_t *buffer, int32_t numFrames) {
        if (!mFileStream.is_open()) {
            throw std::runtime_error("MultiChannelFileRecording: File is not open for writing.");
        }

        // Create a temporary buffer for float conversion.
        std::vector<float> floatBuffer(numFrames * mChannelCount);
        for (int i = 0; i < numFrames * mChannelCount; i++) {
            floatBuffer[i] = static_cast<float>(buffer[i]) * (1.0f / 32768.0f);
        }

        // Seek to the end of the file before writing (append mode).
        // For fstream, seekp(0, std::ios::end) is needed as ios::app is not always consistent across systems.
        mFileStream.seekp(0, std::ios::end);

        // Write the data.
        mFileStream.write(reinterpret_cast<const char*>(floatBuffer.data()),
                          numFrames * mChannelCount * sizeof(float));

        if (!mFileStream) {
            throw std::runtime_error("MultiChannelFileRecording: Failed to write data to file (int16_t conversion).");
        }

        mWriteCursorFrames += numFrames; // Update the conceptual write cursor (file size)
        return numFrames;
    }

    /**
     * @brief Writes 'numFrames' from a float buffer into the recording file.
     * Data is appended to the end of the file.
     * @param buffer A pointer to the source float audio data.
     * @param numFrames The number of frames to write.
     * @return The number of frames actually written (should be 'numFrames').
     * @throws std::runtime_error if the write operation fails.
     */
    int32_t write(float *buffer, int32_t numFrames) {
        if (!mFileStream.is_open()) {
            throw std::runtime_error("MultiChannelFileRecording: File is not open for writing.");
        }

        // Seek to the end of the file before writing (append mode).
        mFileStream.seekp(0, std::ios::end);

        // Write the data.
        mFileStream.write(reinterpret_cast<const char*>(buffer),
                          numFrames * mChannelCount * sizeof(float));

        if (!mFileStream) {
            throw std::runtime_error("MultiChannelFileRecording: Failed to write data to file (float).");
        }

        mWriteCursorFrames += numFrames; // Update the conceptual write cursor (file size)
        return numFrames;
    }

    /**
     * @brief Reads 'numFrames' from the recording file into the provided float buffer.
     * Reading starts from the current conceptual read cursor position.
     * @param buffer A pointer to the destination float buffer.
     * @param numFrames The maximum number of frames to read.
     * @return The number of frames actually read. This may be less than 'numFrames'
     * if insufficient data is available from the current read position to the end of the file.
     * @throws std::runtime_error if the read operation fails.
     */
    int32_t read(float *buffer, int32_t numFrames) {
        if (!mFileStream.is_open()) {
            throw std::runtime_error("MultiChannelFileRecording: File is not open for reading.");
        }

        // Calculate available frames from current read cursor to end of file.
        int64_t availableFrames = mWriteCursorFrames - mReadCursorFrames;
        int32_t framesToRead = (int32_t) std::min(static_cast<int64_t>(numFrames), availableFrames);

        if (framesToRead <= 0) {
            return 0; // No frames to read
        }

        // Seek to the current read cursor position.
        mFileStream.seekg(mReadCursorFrames * mChannelCount * sizeof(float));

        // Read the data.
        mFileStream.read(reinterpret_cast<char*>(buffer),
                         framesToRead * mChannelCount * sizeof(float));

        if (!mFileStream && !mFileStream.eof()) { // Check for read error, but not EOF
            throw std::runtime_error("MultiChannelFileRecording: Failed to read data from file.");
        }

        mReadCursorFrames += framesToRead; // Advance conceptual read cursor
        return framesToRead;
    }

    /**
     * @brief Seeks the read cursor to a specific conceptual frame position within the file.
     * The position is clamped to within the valid range of the file (0 to mWriteCursorFrames).
     * @param position The target conceptual frame position (64-bit).
     */
    void seek(int64_t position) {
        // Clamp the position to be within the file bounds (0 to mWriteCursorFrames).
        mReadCursorFrames = std::max(static_cast<int64_t>(0), position);
        mReadCursorFrames = std::min(mReadCursorFrames, mWriteCursorFrames);
    }

    /**
     * @brief Returns the current conceptual read cursor position.
     * @return The 64-bit conceptual frame position of the read cursor.
     */
    int64_t tell() {
        return mReadCursorFrames;
    }

private:
    std::fstream    mFileStream;            // File stream for I/O
    const int32_t   mChannelCount;            // Number of audio channels
    const std::string mFilename;            // Name of the file being recorded to/read from

    int64_t         mReadCursorFrames = 0;    // Conceptual read cursor (64-bit, file offset in frames)
    int64_t         mWriteCursorFrames = 0;   // Conceptual write cursor (64-bit, current file size in frames)
};

#endif //NATIVEOBOE_MULTICHANNEL_FILE_RECORDING_H
