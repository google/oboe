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

} // namespace oboe

#endif // OBOE_DEFINITIONS_H
