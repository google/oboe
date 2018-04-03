/*
 * Copyright 2018 The Android Open Source Project
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

#include "SoundRecording.h"
#include "utils/logging.h"

void SoundRecording::renderAudio(int16_t *targetData, int32_t numFrames){

    if (mIsPlaying){

        // Check whether we're about to reach the end of the recording
        if (!mIsLooping && mReadFrameIndex + numFrames >= mTotalFrames){
            numFrames = mTotalFrames - mReadFrameIndex;
            mIsPlaying = false;
        }

        for (int i = 0; i < numFrames; ++i) {
            for (int j = 0; j < mChannelCount; ++j) {
                targetData[(i*mChannelCount)+j] = mData[(mReadFrameIndex*mChannelCount)+j];
            }

            // Increment and handle wraparound
            if (++mReadFrameIndex >= mTotalFrames) mReadFrameIndex = 0;
        }

    } else {
        // fill with zeros to output silence
        for (int i = 0; i < numFrames * mChannelCount; ++i) {
            targetData[i] = 0;
        }
    }
}

SoundRecording * SoundRecording::loadFromAssets(AAssetManager *assetManager, const char *filename) {

    // Load the backing track
    AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);

    if (asset == nullptr){
        LOGE("Failed to open track, filename %s", filename);
        return nullptr;
    }

    // Get the length of the track (we assume it is stereo 48kHz)
    off_t trackLength = AAsset_getLength(asset);

    // Load it into memory
    const int16_t *audioBuffer = static_cast<const int16_t*>(AAsset_getBuffer(asset));

    if (audioBuffer == nullptr){
        LOGE("Could not get buffer for track");
        return nullptr;
    }

    // There are 4 bytes per frame because
    // each sample is 2 bytes and
    // it's a stereo recording which has 2 samples per frame.
    int32_t numFrames = (int32_t) (trackLength / 4);
    LOGD("Opened backing track, bytes: %ld frames: %d", trackLength, numFrames);
    return new SoundRecording(audioBuffer, numFrames);
}
