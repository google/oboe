/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OBOE_DEFINITIONS_H
#define OBOE_DEFINITIONS_H

#include <cstdint>
#include <type_traits>
#include <aaudio/AAudio.h>

// Ensure that all AAudio primitive data types are int32_t
#define ASSERT_INT32(type) static_assert(std::is_same<int32_t, type>::value, \
#type" must be int32_t")

ASSERT_INT32(aaudio_stream_state_t);
ASSERT_INT32(aaudio_direction_t);
ASSERT_INT32(aaudio_format_t);
ASSERT_INT32(aaudio_data_callback_result_t);
ASSERT_INT32(aaudio_result_t);
ASSERT_INT32(aaudio_sharing_mode_t);
ASSERT_INT32(aaudio_performance_mode_t);

namespace oboe {

    /**
     * Represents any attribute, property or value which hasn't been specified.
     */
    constexpr int32_t kUnspecified = 0;

    // TODO: Investigate using std::chrono
    /**
     * The number of nanoseconds in a microsecond. 1,000.
     */
    constexpr int64_t kNanosPerMicrosecond =    1000;

    /**
     * The number of nanoseconds in a millisecond. 1,000,000.
     */
    constexpr int64_t kNanosPerMillisecond =    kNanosPerMicrosecond * 1000;

    /**
     * The number of milliseconds in a second. 1,000.
     */
    constexpr int64_t kMillisPerSecond =        1000;

    /**
     * The number of nanoseconds in a second. 1,000,000,000.
     */
    constexpr int64_t kNanosPerSecond =         kNanosPerMillisecond * kMillisPerSecond;

    /**
     * The state of the audio stream.
     */
    enum class StreamState : aaudio_stream_state_t {
        Uninitialized = AAUDIO_STREAM_STATE_UNINITIALIZED,
        Unknown = AAUDIO_STREAM_STATE_UNKNOWN,
        Open = AAUDIO_STREAM_STATE_OPEN,
        Starting = AAUDIO_STREAM_STATE_STARTING,
        Started = AAUDIO_STREAM_STATE_STARTED,
        Pausing = AAUDIO_STREAM_STATE_PAUSING,
        Paused = AAUDIO_STREAM_STATE_PAUSED,
        Flushing = AAUDIO_STREAM_STATE_FLUSHING,
        Flushed = AAUDIO_STREAM_STATE_FLUSHED,
        Stopping = AAUDIO_STREAM_STATE_STOPPING,
        Stopped = AAUDIO_STREAM_STATE_STOPPED,
        Closing = AAUDIO_STREAM_STATE_CLOSING,
        Closed = AAUDIO_STREAM_STATE_CLOSED,
        Disconnected = AAUDIO_STREAM_STATE_DISCONNECTED,
    };

    /**
     * The direction of the stream.
     */
    enum class Direction : aaudio_direction_t {

        /**
         * Used for playback.
         */
        Output = AAUDIO_DIRECTION_OUTPUT,

        /**
         * Used for recording.
         */
        Input = AAUDIO_DIRECTION_INPUT,
    };

    /**
     * The format of audio samples.
     */
    enum class AudioFormat : aaudio_format_t {
        /**
         * Invalid format.
         */
        Invalid = AAUDIO_FORMAT_INVALID,

        /**
         * Unspecified format. Format will be decided by Oboe.
         */
        Unspecified = AAUDIO_FORMAT_UNSPECIFIED,

        /**
         * Signed 16-bit integers.
         */
        I16 = AAUDIO_FORMAT_PCM_I16,

        /**
         * Single precision floating points.
         */
        Float = AAUDIO_FORMAT_PCM_FLOAT,
    };

    /**
     * The result of an audio callback.
     */
    enum class DataCallbackResult : aaudio_data_callback_result_t {
        // Indicates to the caller that the callbacks should continue.
        Continue = AAUDIO_CALLBACK_RESULT_CONTINUE,

        // Indicates to the caller that the callbacks should stop immediately.
        Stop = AAUDIO_CALLBACK_RESULT_STOP,
    };

    /**
     * The result of an operation. All except the `OK` result indicates that an error occurred.
     * The `Result` can be converted into a human readable string using `convertToText`.
     */
    enum class Result : aaudio_result_t {
        OK,
        ErrorBase = AAUDIO_ERROR_BASE,
        ErrorDisconnected = AAUDIO_ERROR_DISCONNECTED,
        ErrorIllegalArgument = AAUDIO_ERROR_ILLEGAL_ARGUMENT,
        ErrorInternal = AAUDIO_ERROR_INTERNAL,
        ErrorInvalidState = AAUDIO_ERROR_INVALID_STATE,
        ErrorInvalidHandle = AAUDIO_ERROR_INVALID_HANDLE,
        ErrorUnimplemented = AAUDIO_ERROR_UNIMPLEMENTED,
        ErrorUnavailable = AAUDIO_ERROR_UNAVAILABLE,
        ErrorNoFreeHandles = AAUDIO_ERROR_NO_FREE_HANDLES,
        ErrorNoMemory = AAUDIO_ERROR_NO_MEMORY,
        ErrorNull = AAUDIO_ERROR_NULL,
        ErrorTimeout = AAUDIO_ERROR_TIMEOUT,
        ErrorWouldBlock = AAUDIO_ERROR_WOULD_BLOCK,
        ErrorInvalidFormat = AAUDIO_ERROR_INVALID_FORMAT,
        ErrorOutOfRange = AAUDIO_ERROR_OUT_OF_RANGE,
        ErrorNoService = AAUDIO_ERROR_NO_SERVICE,
        ErrorInvalidRate = AAUDIO_ERROR_INVALID_RATE,
        // Reserved for future AAudio result types
        Reserved1,
        Reserved2,
        Reserved3,
        Reserved4,
        Reserved5,
        Reserved6,
        Reserved7,
        Reserved8,
        Reserved9,
        Reserved10,
        ErrorClosed,
    };

    /**
     * The sharing mode of the audio stream.
     */
    enum class SharingMode : aaudio_sharing_mode_t {

        /**
         * This will be the only stream using a particular source or sink.
         * This mode will provide the lowest possible latency.
         * You should close EXCLUSIVE streams immediately when you are not using them.
         *
         * If you do not need the lowest possible latency then we recommend using Shared,
         * which is the default.
         */
        Exclusive = AAUDIO_SHARING_MODE_EXCLUSIVE,

        /**
         * Multiple applications can share the same device.
         * The data from output streams will be mixed by the audio service.
         * The data for input streams will be distributed by the audio service.
         *
         * This will have higher latency than the EXCLUSIVE mode.
         */
        Shared = AAUDIO_SHARING_MODE_SHARED,
    };

    /**
     * The performance mode of the audio stream.
     */
    enum class PerformanceMode : aaudio_performance_mode_t {

        /**
         * No particular performance needs. Default.
         */
        None = AAUDIO_PERFORMANCE_MODE_NONE,

        /**
         * Extending battery life is most important.
         */
        PowerSaving = AAUDIO_PERFORMANCE_MODE_POWER_SAVING,

        /**
         * Reducing latency is most important.
         */
        LowLatency = AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
    };

    /**
     * The underlying audio API used by the audio stream.
     */
    enum class AudioApi : int32_t {
        /**
         * Try to use AAudio. If not available then use OpenSL ES.
         */
        Unspecified = kUnspecified,

        /**
         * Use OpenSL ES.
         */
        OpenSLES,

        /**
         * Try to use AAudio. Fail if unavailable.
         */
        AAudio
    };

// Hard code constants so they can be compiled with versions of the NDK before P.
#if __ANDROID_API_LEVEL__ >= __ANDROID_API_P__
#define CONSTANT_API_P(hard_constant, soft_constant) (soft_constant)
#else
#define CONSTANT_API_P(hard_constant, soft_constant) (hard_constant)
#endif

    /**
     * The Usage attribute expresses *why* you are playing a sound, what is this sound used for.
     * This information is used by certain platforms or routing policies
     * to make more refined volume or routing decisions.
     *
     * Note that these match the equivalent values in AudioAttributes in the Android Java API.
     *
     * This attribute only has an effect on Android API 28+.
     */
    enum class Usage : aaudio_usage_t {
        /**
         * Use this for streaming media, music performance, video, podcasts, etcetera.
         */
        Media = CONSTANT_API_P(1, AAUDIO_USAGE_MEDIA),

        /**
         * Use this for voice over IP, telephony, etcetera.
         */
        VoiceCommunication = CONSTANT_API_P(2, AAUDIO_USAGE_VOICE_COMMUNICATION),

        /**
         * Use this for sounds associated with telephony such as busy tones, DTMF, etcetera.
         */
        VoiceCommunicationSignalling = CONSTANT_API_P(3,
                                                      AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING),

        /**
         * Use this to demand the users attention.
         */
        Alarm = CONSTANT_API_P(4, AAUDIO_USAGE_ALARM),

        /**
         * Use this for notifying the user when a message has arrived or some
         * other background event has occured.
         */
        Notification = CONSTANT_API_P(5, AAUDIO_USAGE_NOTIFICATION),

        /**
         * Use this when the phone rings.
         */
        NotificationRingtone = CONSTANT_API_P(6, AAUDIO_USAGE_NOTIFICATION_RINGTONE),

        /**
         * Use this to attract the users attention when, for example, the battery is low.
         */
        NotificationEvent = CONSTANT_API_P(10, AAUDIO_USAGE_NOTIFICATION_EVENT),

        /**
         * Use this for screen readers, etcetera.
         */
        AssistanceAccessibility = CONSTANT_API_P(11, AAUDIO_USAGE_ASSISTANCE_ACCESSIBILITY),

        /**
         * Use this for driving or navigation directions.
         */
        AssistanceNavigationGuidance = CONSTANT_API_P(12,
                                                      AAUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE),

        /**
         * Use this for user interface sounds, beeps, etcetera.
         */
        AssistanceSonification = CONSTANT_API_P(13, AAUDIO_USAGE_ASSISTANCE_SONIFICATION),

        /**
         * Use this for game audio and sound effects.
         */
        Game = CONSTANT_API_P(14, AAUDIO_USAGE_GAME),

        /**
         * Use this for audio responses to user queries, audio instructions or help utterances.
         */
        Assistant = CONSTANT_API_P(16, AAUDIO_USAGE_ASSISTANT),
    };


    /**
     * The ContentType attribute describes *what* you are playing.
     * It expresses the general category of the content. This information is optional.
     * But in case it is known (for instance {@link Movie} for a
     * movie streaming service or {@link Speech} for
     * an audio book application) this information might be used by the audio framework to
     * enforce audio focus.
     *
     * Note that these match the equivalent values in AudioAttributes in the Android Java API.
     *
     * This attribute only has an effect on Android API 28+.
     */
    enum ContentType : aaudio_content_type_t {

        /**
         * Use this for spoken voice, audio books, etcetera.
         */
        Speech = CONSTANT_API_P(1, AAUDIO_CONTENT_TYPE_SPEECH),

        /**
         * Use this for pre-recorded or live music.
         */
        Music = CONSTANT_API_P(2, AAUDIO_CONTENT_TYPE_MUSIC),

        /**
         * Use this for a movie or video soundtrack.
         */
        Movie = CONSTANT_API_P(3, AAUDIO_CONTENT_TYPE_MOVIE),

        /**
         * Use this for sound is designed to accompany a user action,
         * such as a click or beep sound made when the user presses a button.
         */
        Sonification = CONSTANT_API_P(4, AAUDIO_CONTENT_TYPE_SONIFICATION),
    };

    /**
     * Defines the audio source.
     * An audio source defines both a default physical source of audio signal, and a recording
     * configuration.
     *
     * Note that these match the equivalent values in MediaRecorder.AudioSource in the Android Java API.
     *
     * This attribute only has an effect on Android API 28+.
     */
    enum InputPreset : aaudio_input_preset_t {
        /**
         * Use this preset when other presets do not apply.
         */
        Generic = CONSTANT_API_P(1, AAUDIO_INPUT_PRESET_GENERIC),

        /**
         * Use this preset when recording video.
         */
        Camcorder = CONSTANT_API_P(5, AAUDIO_INPUT_PRESET_CAMCORDER),

        /**
         * Use this preset when doing speech recognition.
         */
        VoiceRecognition = CONSTANT_API_P(6, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION),

        /**
         * Use this preset when doing telephony or voice messaging.
         */
        VoiceCommunication = CONSTANT_API_P(7, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION),

        /**
         * Use this preset to obtain an input with no effects.
         * Note that this input will not have automatic gain control
         * so the recorded volume may be very low.
         */
        Unprocessed = CONSTANT_API_P(9, AAUDIO_INPUT_PRESET_UNPROCESSED),
    };

    /**
     * This attribute can be used to allocate a session ID to the audio stream.
     *
     * This attribute only has an effect on Android API 28+.
     */
    enum SessionId {
        /**
         * Do not allocate a session ID.
         * Effects cannot be used with this stream.
         * Default.
         */
         None = CONSTANT_API_P(-1, AAUDIO_SESSION_ID_NONE),

        /**
         * Allocate a session ID that can be used to attach and control
         * effects using the Java AudioEffects API.
         * Note that the use of this flag may result in higher latency.
         *
         * Note that this matches the value of AudioManager.AUDIO_SESSION_ID_GENERATE.
         */
         Allocate = CONSTANT_API_P(0, AAUDIO_SESSION_ID_ALLOCATE),
    };

    /**
     * The channel count of the audio stream. The underlying type is `int32_t`.
     * Use of this enum is convenient to avoid "magic"
     * numbers when specifying the channel count.
     *
     * For example, you can write
     * `builder.setChannelCount(ChannelCount::Stereo)`
     * rather than `builder.setChannelCount(2)`
     *
     */
    enum ChannelCount : int32_t {
      /**
       * Audio channel count definition, use Mono or Stereo
       */
      Unspecified = kUnspecified,

      /**
       * Use this for mono audio
       */
      Mono = 1,

      /**
       * Use this for stereo audio.
       */
      Stereo = 2,
    };

#undef CONSTANT_API_P

    /**
     * On API 16 to 26 OpenSL ES will be used. When using OpenSL ES the optimal values for sampleRate and
     * framesPerBurst are not known by the native code.
     * On API 17+ these values should be obtained from the AudioManager using this code:
     *
     * <pre><code>
        // Note that this technique only works for built-in speakers and headphones.
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        String sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        int defaultSampleRate = Integer.parseInt(sampleRateStr);
        String framesPerBurstStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        int defaultFramesPerBurst = Integer.parseInt(framesPerBurstStr);
        </code></pre>
     *
     * It can then be passed down to Oboe through JNI.
     *
     * AAudio will get the optimal framesPerBurst from the HAL and will ignore this value.
     */
    class DefaultStreamValues {

    public:

        /** The default sample rate to use when opening new audio streams */
        static int32_t SampleRate;
        /** The default frames per burst to use when opening new audio streams */
        static int32_t FramesPerBurst;
        /** The default channel count to use when opening new audio streams */
        static int32_t ChannelCount;

    };

    /**
     * The time at which the frame at `position` was presented
     */
    struct FrameTimestamp {
        int64_t position; // in frames
        int64_t timestamp; // in nanoseconds
    };


} // namespace oboe

#endif // OBOE_DEFINITIONS_H
