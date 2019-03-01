/*
 * Copyright (C) 2018 The Android Open Source Project
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


#include <utils/logging.h>
#include <oboe/Oboe.h>
#include "AAssetDataSource.h"


AAssetDataSource* AAssetDataSource::newFromAssetManager(AAssetManager &assetManager,
                                                        const char *filename,
                                                        const int32_t channelCount) {

    // Load the backing track
    AAsset* asset = AAssetManager_open(&assetManager, filename, AASSET_MODE_BUFFER);

    if (asset == nullptr){
        LOGE("Failed to open track, filename %s", filename);
        return nullptr;
    }

    // Get the length of the track (we assume it is stereo 48kHz)
    off_t trackSizeInBytes = AAsset_getLength(asset);
    auto numSamples = static_cast<int32_t>(trackSizeInBytes / sizeof(int16_t));
    auto numFrames = static_cast<int32_t>(numSamples / channelCount);

    // Load it into memory (we assume it is 16 bit signed integers)
    auto *sourceBuffer = static_cast<const int16_t*>(AAsset_getBuffer(asset));

    if (sourceBuffer == nullptr){
        LOGE("Could not get buffer for track");
        return nullptr;
    }

    auto outputBuffer = std::make_unique<float[]>(numSamples);
    oboe::convertPcm16ToFloat(sourceBuffer, outputBuffer.get(), numSamples);

    LOGD("Opened audio data source %s, bytes: %ld samples: %d frames: %d", filename, trackSizeInBytes, numSamples, numFrames);

    AAsset_close(asset);

    return new AAssetDataSource(std::move(outputBuffer), numFrames, channelCount);
}
