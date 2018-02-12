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

#ifndef OBOE_STREAM_BUILDER_H_
#define OBOE_STREAM_BUILDER_H_

#include "oboe/Definitions.h"
#include "oboe/AudioStreamBase.h"

namespace oboe {

constexpr int32_t kDefaultFramesPerBurst = 192; // arbitrary value, 4 msec at 48000 Hz

/**
 * Factory class for an audio Stream.
 */
class AudioStreamBuilder : public AudioStreamBase {
public:

    AudioStreamBuilder() : AudioStreamBase() {}

    enum class AudioApi {
        /**
         * Try to use AAudio. If not available then use OpenSL ES.
         */
        Unspecified,

        /**
         * Use OpenSL ES.
         */
        OpenSLES,

        /**
         * Try to use AAudio. Fail if unavailable.
         */
        AAudio
    };


    /**
     * Request a specific number of channels.
     *
     * Default is kUnspecified. If the value is unspecified then
     * the application should query for the actual value after the stream is opened.
     */
    AudioStreamBuilder *setChannelCount(int channelCount) {
        mChannelCount = channelCount;
        return this;
    }

    /**
     * Request the direction for a stream. The default is Direction::Output.
     *
     * @param direction Direction::Output or Direction::Input
     */
    AudioStreamBuilder *setDirection(Direction direction) {
        mDirection = direction;
        return this;
    }

    /**
     * Request a specific sample rate in Hz.
     *
     * Default is kUnspecified. If the value is unspecified then
     * the application should query for the actual value after the stream is opened.
     *
     * Technically, this should be called the "frame rate" or "frames per second",
     * because it refers to the number of complete frames transferred per second.
     * But it is traditionally called "sample rate". Se we use that term.
     *
     */
    AudioStreamBuilder *setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
        return this;
    }

    /**
     * Request a specific number of frames for the data callback.
     *
     * Default is kUnspecified. If the value is unspecified then
     * the actual number may vary from callback to callback.
     *
     * If an application can handle a varying number of frames then we recommend
     * leaving this unspecified. This allow the underlying API to optimize
     * the callbacks. But if your application is, for example, doing FFTs or other block
     * oriented operations, then call this function to get the sizes you need.
     *
     * @param framesPerCallback
     * @return
     */
    AudioStreamBuilder *setFramesPerCallback(int framesPerCallback) {
        mFramesPerCallback = framesPerCallback;
        return this;
    }

    /**
     * Request a sample data format, for example Format::Float.
     *
     * Default is Format::Unspecified. If the value is unspecified then
     * the application should query for the actual value after the stream is opened.
     */
    AudioStreamBuilder *setFormat(AudioFormat format) {
        mFormat = format;
        return this;
    }

    /**
     * Set the requested maximum buffer capacity in frames.
     * The final stream capacity may differ, but will probably be at least this big.
     *
     * Default is kUnspecified.
     *
     * @param frames the desired buffer capacity in frames or kUnspecified
     * @return pointer to the builder so calls can be chained
     */
    AudioStreamBuilder *setBufferCapacityInFrames(int32_t bufferCapacityInFrames) {
        mBufferCapacityInFrames = bufferCapacityInFrames;
        return this;
    }

    AudioApi getApiIndex() const { return mAudioApi; }

    /**
     * Normally you would leave this unspecified, and Oboe will chose the best API
     * for the device at runtime.
     * @param Must be AudioApi::Unspecified, AudioApi::OpenSLES or AudioApi::AAudio.
     * @return pointer to the builder so calls can be chained
     */
    AudioStreamBuilder *setApiIndex(AudioApi apiIndex) {
        mAudioApi = apiIndex;
        return this;
    }

    /**
     * Is the AAudio API supported on this device?
     *
     * AAudio was introduced in the Oreo release.
     *
     * @return true if supported
     */
    static bool isAAudioSupported();

    /**
     * Request a mode for sharing the device.
     * The requested sharing mode may not be available.
     * So the application should query for the actual mode after the stream is opened.
     *
     * @param sharingMode SharingMode::Shared or SharingMode::Exclusive
     * @return pointer to the builder so calls can be chained
     */
    AudioStreamBuilder *setSharingMode(SharingMode sharingMode) {
        mSharingMode = sharingMode;
        return this;
    }

    /**
     * Request a performance level for the stream.
     * This will determine the latency, the power consumption, and the level of
     * protection from glitches.
     *
     * @param performanceMode for example, PerformanceMode::LowLatency
     * @return pointer to the builder so calls can be chained
     */
    AudioStreamBuilder *setPerformanceMode(PerformanceMode performanceMode) {
        mPerformanceMode = performanceMode;
        return this;
    }

    /**
     * Request an audio device identified device using an ID.
     * On Android, for example, the ID could be obtained from the Java AudioManager.
     *
     * By default, the primary device will be used.
     *
     * @param deviceId device identifier or kUnspecified
     * @return pointer to the builder so calls can be chained
     */
    AudioStreamBuilder *setDeviceId(int32_t deviceId) {
        mDeviceId = deviceId;
        return this;
    }

    /**
     * Specifies an object to handle data or error related callbacks from the underlying API.
     *
     * When an error callback occurs, the associated stream will be stopped
     * and closed in a separate thread.
     *
     * @param streamCallback
     * @return
     */
    AudioStreamBuilder *setCallback(std::shared_ptr<AudioStreamCallback> streamCallback) {
        mStreamCallback = streamCallback;
        return this;
    }

    /**
     * With OpenSL ES, the optimal framesPerBurst is not known by the native code.
     * It should be obtained from the AudioManager using this code:
     *
     * <pre><code>
        // Note that this technique only works for built-in speakers and headphones.
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        text = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        defaultFramesPerBurst = Integer.parseInt(text);
        </code></pre>
     *
     * It can then be passed down to Oboe through JNI.
     *
     * AAudio will get the optimal framesPerBurst from the HAL and will ignore this value.
     *
     * @param defaultFramesPerBurst
     * @return pointer to the builder so calls can be chained
     */
    AudioStreamBuilder *setDefaultFramesPerBurst(int32_t defaultFramesPerBurst) {
        mDefaultFramesPerBurst = defaultFramesPerBurst;
        return this;
    }

    int32_t getDefaultFramesPerBurst() const {
        return mDefaultFramesPerBurst;
    }

    /**
     * Create and open a stream object based on the current settings.
     *
     * @param stream pointer to a variable to receive the stream address
     * @return OBOE_OK if successful or a negative error code
     */
    Result openStream(AudioStream **stream);

protected:

private:

    /**
     * Create an AudioStream object. The AudioStream must be opened before use.
     *
     * @return pointer to an AudioStream object.
     */
    oboe::AudioStream *build();

    AudioApi       mAudioApi = AudioApi::Unspecified;

    int32_t        mDefaultFramesPerBurst = kDefaultFramesPerBurst;
};

} // namespace oboe

#endif /* OBOE_STREAM_BUILDER_H_ */
