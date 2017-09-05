/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef OBOE_OBOE_STREAM_BASE_H_
#define OBOE_OBOE_STREAM_BASE_H_

#include "oboe/OboeStreamCallback.h"
#include "oboe/OboeDefinitions.h"

/**
 * Base class for Oboe streams and builders.
 */
class OboeStreamBase {
public:

    OboeStreamBase() {}

    virtual ~OboeStreamBase() = default;

    // This class only contains primitives so we can use default constructor and copy methods.
    OboeStreamBase(const OboeStreamBase&) = default;

    OboeStreamBase& operator=(const OboeStreamBase&) = default;

    int getChannelCount() const { return mChannelCount; }

    oboe_direction_t getDirection() const { return mDirection; }

    int32_t getSampleRate() const { return mSampleRate; }

    int getFramesPerCallback() const { return mFramesPerCallback; }

    oboe_audio_format_t getFormat() const { return mFormat; }

    virtual int32_t getBufferCapacityInFrames() const { return mBufferCapacityInFrames; }

    oboe_sharing_mode_t getSharingMode() const { return mSharingMode; }

    oboe_performance_mode_t getPerformanceMode() const { return mPerformanceMode; }

    int32_t getDeviceId() const { return mDeviceId; }

    OboeStreamCallback *getCallback() const {
        return mStreamCallback;
    }

protected:
    OboeStreamCallback     *mStreamCallback = NULL;
    int32_t                 mFramesPerCallback = OBOE_UNSPECIFIED;
    int32_t                 mChannelCount = OBOE_UNSPECIFIED;
    int32_t                 mSampleRate = OBOE_UNSPECIFIED;
    int32_t                 mDeviceId = OBOE_UNSPECIFIED;
    int32_t                 mBufferCapacityInFrames = OBOE_UNSPECIFIED;

    oboe_sharing_mode_t     mSharingMode = OBOE_SHARING_MODE_SHARED;
    oboe_audio_format_t     mFormat = OBOE_AUDIO_FORMAT_PCM_FLOAT;
    oboe_direction_t        mDirection = OBOE_DIRECTION_OUTPUT;
    oboe_performance_mode_t mPerformanceMode = OBOE_PERFORMANCE_MODE_NONE;
};

#endif /* OBOE_OBOE_STREAM_BASE_H_ */
