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

#ifndef OBOE_OBOE_STREAM_BUILDER_H_
#define OBOE_OBOE_STREAM_BUILDER_H_

#include "OboeStreamBase.h"
#include "OboeStream.h"
#include "oboe/OboeDefinitions.h"

/**
 * Factory class for an AudioStream.
 */
class OboeStreamBuilder : public OboeStreamBase {
public:

    OboeStreamBuilder() : OboeStreamBase() {}

    enum audio_api_index_t {
        /**
         * Try to use AAudio. If not available then use OpenSL ES.
         */
        API_UNSPECIFIED,

        /**
         * Use OpenSL ES.
         */
        API_OPENSL_ES,

        /**
         * Try to use AAudio. Fail if unavailable.
         */
        API_AAUDIO
    };


    /**
     * Request a specific number of channels.
     *
     * Default is OBOE_UNSPECIFIED. If the value is unspecified then
     * the application should query for the actual value after the stream is opened.
     */
    OboeStreamBuilder *setChannelCount(int channelCount) {
        mChannelCount = channelCount;
        return this;
    }

    /**
     * Request the direction for a stream. The default is OBOE_DIRECTION_OUTPUT.
     *
     * @param direction OBOE_DIRECTION_OUTPUT or OBOE_DIRECTION_INPUT
     */
    OboeStreamBuilder *setDirection(oboe_direction_t direction) {
        mDirection = direction;
        return this;
    }

    /**
     * Request a specific sample rate in Hz.
     *
     * Default is OBOE_UNSPECIFIED. If the value is unspecified then
     * the application should query for the actual value after the stream is opened.
     *
     * Technically, this should be called the "frame rate" or "frames per second",
     * because it refers to the number of complete frames transferred per second.
     * But it is traditionally called "sample rate". Se we use that term.
     *
     */
    OboeStreamBuilder *setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
        return this;
    }

    /**
     * Request a specific number of frames for the data callback.
     *
     * Default is OBOE_UNSPECIFIED. If the value is unspecified then
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
    OboeStreamBuilder *setFramesPerCallback(int framesPerCallback) {
        mFramesPerCallback = framesPerCallback;
        return this;
    }

    /**
     * Request a sample data format, for example OBOE_FORMAT_PCM_FLOAT.
     *
     * Default is OBOE_UNSPECIFIED. If the value is unspecified then
     * the application should query for the actual value after the stream is opened.
     */
    OboeStreamBuilder *setFormat(oboe_audio_format_t format) {
        mFormat = format;
        return this;
    }

    /**
     * Set the requested maximum buffer capacity in frames.
     * The final stream capacity may differ, but will probably be at least this big.
     *
     * Default is OBOE_UNSPECIFIED.
     *
     * @param frames the desired buffer capacity in frames or OBOE_UNSPECIFIED
     * @return pointer to the builder so calls can be chained
     */
    OboeStreamBuilder *setBufferCapacityInFrames(int32_t bufferCapacityInFrames) {
        mBufferCapacityInFrames = bufferCapacityInFrames;
        return this;
    }

    audio_api_index_t getApiIndex() const { return mAudioApi; }

    /**
     * Normally you would leave this unspecified, and Oboe will chose the best API
     * for the device at runtime.
     * @param Must be API_UNSPECIFIED, API_OPENSL_ES or API_AAUDIO.
     * @return pointer to the builder so calls can be chained
     */
    OboeStreamBuilder *setApiIndex(audio_api_index_t apiIndex) {
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
     * @param sharingMode OBOE_SHARING_MODE_LEGACY or OBOE_SHARING_MODE_EXCLUSIVE
     * @return pointer to the builder so calls can be chained
     */
    OboeStreamBuilder *setSharingMode(oboe_sharing_mode_t sharingMode) {
        mSharingMode = sharingMode;
        return this;
    }

    /**
     * Request a performance level for the stream.
     * This will determine the latency, the power consumption, and the level of
     * protection from glitches.
     *
     * @param performanceMode for example, OBOE_PERFORMANCE_MODE_LOW_LATENCY
     * @return pointer to the builder so calls can be chained
     */
    OboeStreamBuilder *setPerformanceMode(oboe_performance_mode_t performanceMode) {
        mPerformanceMode = performanceMode;
        return this;
    }

    /**
     * Request an audio device identified device using an ID.
     * On Android, for example, the ID could be obtained from the Java AudioManager.
     *
     * By default, the primary device will be used.
     *
     * @param deviceId device identifier or OBOE_DEVICE_UNSPECIFIED
     * @return pointer to the builder so calls can be chained
     */
    OboeStreamBuilder *setDeviceId(int32_t deviceId) {
        mDeviceId = deviceId;
        return this;
    }

    OboeStreamBuilder *setCallback(OboeStreamCallback *streamCallback) {
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
    OboeStreamBuilder *setDefaultFramesPerBurst(int32_t defaultFramesPerBurst) {
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
    oboe_result_t openStream(OboeStream **stream);

protected:

private:

    /**
     * Create an AudioStream object. The AudioStream must be opened before use.
     *
     * @return pointer to an AudioStream object.
     */
    OboeStream *build();

    audio_api_index_t       mAudioApi = API_UNSPECIFIED;

    int32_t                 mDefaultFramesPerBurst = 192; // arbitrary value, 4 msec at 48000 Hz
};

#endif /* OBOE_OBOE_STREAM_BUILDER_H_ */
