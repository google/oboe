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

    constexpr int32_t kUnspecified = 0;

    // TODO: Investigate using std::chrono
    constexpr int64_t kNanosPerMicrosecond =    1000;
    constexpr int64_t kNanosPerMillisecond =    kNanosPerMicrosecond * 1000;
    constexpr int64_t kMillisPerSecond =        1000;
    constexpr int64_t kNanosPerSecond =         kNanosPerMillisecond * kMillisPerSecond;

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

    enum class Direction : aaudio_direction_t {
        Output = AAUDIO_DIRECTION_OUTPUT,
        Input = AAUDIO_DIRECTION_INPUT,
    };

    enum class AudioFormat : aaudio_format_t {
        Invalid = AAUDIO_FORMAT_INVALID,
        Unspecified = AAUDIO_FORMAT_UNSPECIFIED,
        I16 = AAUDIO_FORMAT_PCM_I16,
        Float = AAUDIO_FORMAT_PCM_FLOAT,
    };

    enum class DataCallbackResult : aaudio_data_callback_result_t {
        Continue = AAUDIO_CALLBACK_RESULT_CONTINUE,
        Stop = AAUDIO_CALLBACK_RESULT_STOP,
    };

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

    enum class SharingMode : aaudio_sharing_mode_t {

        /**
         * This will be the only stream using a particular source or sink.
         * This mode will provide the lowest possible latency.
         * You should close EXCLUSIVE streams immediately when you are not using them.
         */
        Exclusive = AAUDIO_SHARING_MODE_EXCLUSIVE,

        /**
         * Multiple applications will be mixed by the AAudio Server.
         * This will have higher latency than the EXCLUSIVE mode.
         */
        Shared = AAUDIO_SHARING_MODE_SHARED,
    };

    enum class PerformanceMode : aaudio_performance_mode_t {

        // No particular performance needs. Default.
        None = AAUDIO_PERFORMANCE_MODE_NONE,

        // Extending battery life is most important.
        PowerSaving = AAUDIO_PERFORMANCE_MODE_POWER_SAVING,

        // Reducing latency is most important.
        LowLatency = AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
    };

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
     * The Usage attribute expresses "why" you are playing a sound, what is this sound used for.
     * This information is used by certain platforms or routing policies
     * to make more refined volume or routing decisions.
     *
     * Note that these match the equivalent values in AudioAttributes in the Android Java API.
     *
     * Added in API level 28.
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
     * The CONTENT_TYPE attribute describes "what" you are playing.
     * It expresses the general category of the content. This information is optional.
     * But in case it is known (for instance {@link #AAUDIO_CONTENT_TYPE_MOVIE} for a
     * movie streaming service or {@link #AAUDIO_CONTENT_TYPE_SPEECH} for
     * an audio book application) this information might be used by the audio framework to
     * enforce audio focus.
     *
     * Note that these match the equivalent values in AudioAttributes in the Android Java API.
     *
     * Added in API level 28.
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
     * Added in API level 28.
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

    enum SessionId {
        /**
         * Do not allocate a session ID.
         * Effects cannot be used with this stream.
         * Default.
         *
         * Added in API level 28.
         */
         None = CONSTANT_API_P(-1, AAUDIO_SESSION_ID_NONE),

        /**
         * Allocate a session ID that can be used to attach and control
         * effects using the Java AudioEffects API.
         * Note that the use of this flag may result in higher latency.
         *
         * Note that this matches the value of AudioManager.AUDIO_SESSION_ID_GENERATE.
         *
         * Added in API level 28.
         */
         Allocate = CONSTANT_API_P(0, AAUDIO_SESSION_ID_ALLOCATE),
    };

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

    class DefaultStreamValues {

    public:

        /**
         * On API 16 to 25 OpenSL ES will be used. When using OpenSL ES the optimal values for sampleRate and
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
        static int32_t SampleRate;
        static int32_t FramesPerBurst;
        static int32_t ChannelCount;
    };



} // namespace oboe

#endif // OBOE_DEFINITIONS_H
