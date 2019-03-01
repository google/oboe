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

#ifndef RHYTHMGAME_AASSETDATASOURCE_H
#define RHYTHMGAME_AASSETDATASOURCE_H

#include <android/asset_manager.h>
#include "DataSource.h"

class AAssetDataSource : public DataSource {

public:
    int32_t getTotalFrames() const override { return mTotalFrames; }
    int32_t getChannelCount() const override { return mChannelCount; }
    const float* getData() const override { return mBuffer.get();	}

    static AAssetDataSource* newFromAssetManager(AAssetManager&, const char *, const int32_t);

private:

    AAssetDataSource(std::unique_ptr<float[]> data, int32_t frames,
                     const int32_t channelCount)
            : mBuffer(std::move(data))
            , mTotalFrames(frames)
            , mChannelCount(channelCount) {
    }

    const std::unique_ptr<float[]> mBuffer;
    const int32_t mTotalFrames;
    const int32_t mChannelCount;

};
#endif //RHYTHMGAME_AASSETDATASOURCE_H
