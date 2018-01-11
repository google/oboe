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

#ifndef OBOE_STREAM_BASE_H_
#define OBOE_STREAM_BASE_H_

#include <memory>
#include "oboe/AudioStreamCallback.h"
#include "oboe/Definitions.h"

namespace oboe {

/**
 * Base class containing parameters for Oboe streams and builders.
 *
 * OboeStreamBuilder can return OBOE_UNSPECIFIED or the requested value.
 *
 * OboeStream will generally return the actual final value, but getFramesPerCallback()
 * can be unspecified even for a stream.
 */
class AudioStreamBase {
public:

    AudioStreamBase() {}

    virtual ~AudioStreamBase() = default;

    // This class only contains primitives so we can use default constructor and copy methods.
    AudioStreamBase(const AudioStreamBase&) = default;

    AudioStreamBase& operator=(const AudioStreamBase&) = default;

    /**
     * @return number of channels, for example 2 for stereo
     */
    int getChannelCount() const { return mChannelCount; }

    /**
     * @return Direction::Input or Direction::Output
     */
    Direction getDirection() const { return mDirection; }

    /**
     * @return sample rate for the stream
     */
    int32_t getSampleRate() const { return mSampleRate; }

    /**
     * @return framesPerCallback or OBOE_UNSPECIFIED
     */
    int getFramesPerCallback() const { return mFramesPerCallback; }

    /**
     * @return OBOE_AUDIO_FORMAT_PCM_FLOAT, OBOE_AUDIO_FORMAT_PCM_I16
     *         or OBOE_AUDIO_FORMAT_UNSPECIFIED
     */
    AudioFormat getFormat() const { return mFormat; }

    /**
     * Query the maximum number of frames that can be filled without blocking.
     *
     * @return buffer size or a negative error.
     */
    virtual int32_t getBufferSizeInFrames() const {
        // By default assume the effective size is the same as capacity.
        return getBufferCapacityInFrames();
    }

    /**
     * @return capacityInFrames or OBOE_UNSPECIFIED
     */
    virtual int32_t getBufferCapacityInFrames() const { return mBufferCapacityInFrames; }

    SharingMode getSharingMode() const { return mSharingMode; }

    PerformanceMode getPerformanceMode() const { return mPerformanceMode; }

    int32_t getDeviceId() const { return mDeviceId; }

    AudioStreamCallback* getCallback() const {
        return mStreamCallback;
    }

protected:
    AudioStreamCallback            *mStreamCallback = nullptr;
    int32_t                         mFramesPerCallback = kUnspecified;
    int32_t                         mChannelCount = kUnspecified;
    int32_t                         mSampleRate = kUnspecified;
    int32_t                         mDeviceId = kUnspecified;
    int32_t                         mBufferCapacityInFrames = kUnspecified;

    SharingMode                     mSharingMode = SharingMode::Shared;
    AudioFormat                     mFormat = AudioFormat::Unspecified;
    Direction                       mDirection = Direction::Output;
    PerformanceMode                 mPerformanceMode = PerformanceMode::None;
};

} // namespace oboe

#endif /* OBOE_STREAM_BASE_H_ */
