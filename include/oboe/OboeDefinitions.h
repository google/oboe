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

#ifndef OBOE_OBOE_DEFINITIONS_H
#define OBOE_OBOE_DEFINITIONS_H

#include <stdint.h>
#include <aaudio/AAudio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef aaudio_result_t  oboe_result_t;

/**
 * This is used to represent a value that has not been specified.
 * For example, an application could use OBOE_UNSPECIFIED to indicate
 * that it did not not care what the specific value of a parameter was
 * and would accept whatever it was given.
 */
#define OBOE_UNSPECIFIED           0
#define OBOE_NANOS_PER_MICROSECOND ((int64_t)1000)
#define OBOE_NANOS_PER_MILLISECOND (OBOE_NANOS_PER_MICROSECOND * 1000)
#define OBOE_MILLIS_PER_SECOND     1000
#define OBOE_NANOS_PER_SECOND      (OBOE_NANOS_PER_MILLISECOND * OBOE_MILLIS_PER_SECOND)

#define oboe_direction_t aaudio_direction_t
#define OBOE_DIRECTION_OUTPUT AAUDIO_DIRECTION_OUTPUT
#define OBOE_DIRECTION_INPUT AAUDIO_DIRECTION_INPUT

#define oboe_audio_format_t aaudio_format_t
#define OBOE_AUDIO_FORMAT_INVALID     AAUDIO_FORMAT_INVALID
#define OBOE_AUDIO_FORMAT_UNSPECIFIED AAUDIO_FORMAT_UNSPECIFIED
#define OBOE_AUDIO_FORMAT_PCM_I16     AAUDIO_FORMAT_PCM_I16
#define OBOE_AUDIO_FORMAT_PCM_FLOAT   AAUDIO_FORMAT_PCM_FLOAT

#define oboe_data_callback_result_t   aaudio_data_callback_result_t
#define OBOE_CALLBACK_RESULT_CONTINUE AAUDIO_CALLBACK_RESULT_CONTINUE
#define OBOE_CALLBACK_RESULT_STOP     AAUDIO_CALLBACK_RESULT_STOP

enum {
    OBOE_OK,
    OBOE_ERROR_BASE = AAUDIO_ERROR_BASE,
    OBOE_ERROR_DISCONNECTED = AAUDIO_ERROR_DISCONNECTED,
    OBOE_ERROR_ILLEGAL_ARGUMENT = AAUDIO_ERROR_ILLEGAL_ARGUMENT,
    // Reserved
    OBOE_ERROR_INTERNAL = AAUDIO_ERROR_INTERNAL,
    OBOE_ERROR_INVALID_STATE = AAUDIO_ERROR_INVALID_STATE,
    // Reserved
    // Reserved
    OBOE_ERROR_INVALID_HANDLE = AAUDIO_ERROR_INVALID_HANDLE,
    // Reserved
    OBOE_ERROR_UNIMPLEMENTED = AAUDIO_ERROR_UNIMPLEMENTED,
    OBOE_ERROR_UNAVAILABLE = AAUDIO_ERROR_UNAVAILABLE,
    OBOE_ERROR_NO_FREE_HANDLES = AAUDIO_ERROR_NO_FREE_HANDLES,
    OBOE_ERROR_NO_MEMORY = AAUDIO_ERROR_NO_MEMORY,
    OBOE_ERROR_NULL = AAUDIO_ERROR_NULL,
    OBOE_ERROR_TIMEOUT = AAUDIO_ERROR_TIMEOUT,
    OBOE_ERROR_WOULD_BLOCK = AAUDIO_ERROR_WOULD_BLOCK,
    OBOE_ERROR_INVALID_FORMAT = AAUDIO_ERROR_INVALID_FORMAT,
    OBOE_ERROR_OUT_OF_RANGE = AAUDIO_ERROR_OUT_OF_RANGE,
    OBOE_ERROR_NO_SERVICE = AAUDIO_ERROR_NO_SERVICE,
    OBOE_ERROR_INVALID_RATE = AAUDIO_ERROR_INVALID_RATE
};

#define oboe_stream_state_t aaudio_stream_state_t

#define OBOE_STREAM_STATE_UNINITIALIZED AAUDIO_STREAM_STATE_UNINITIALIZED
#define OBOE_STREAM_STATE_OPEN      AAUDIO_STREAM_STATE_OPEN
#define OBOE_STREAM_STATE_STARTING  AAUDIO_STREAM_STATE_STARTING
#define OBOE_STREAM_STATE_STARTED   AAUDIO_STREAM_STATE_STARTED
#define OBOE_STREAM_STATE_PAUSING   AAUDIO_STREAM_STATE_PAUSING
#define OBOE_STREAM_STATE_PAUSED    AAUDIO_STREAM_STATE_PAUSED
#define OBOE_STREAM_STATE_FLUSHING  AAUDIO_STREAM_STATE_FLUSHING
#define OBOE_STREAM_STATE_FLUSHED   AAUDIO_STREAM_STATE_FLUSHED
#define OBOE_STREAM_STATE_STOPPING  AAUDIO_STREAM_STATE_STOPPING
#define OBOE_STREAM_STATE_STOPPED   AAUDIO_STREAM_STATE_STOPPED
#define OBOE_STREAM_STATE_CLOSING   AAUDIO_STREAM_STATE_CLOSING
#define OBOE_STREAM_STATE_CLOSED    AAUDIO_STREAM_STATE_CLOSED

#define oboe_sharing_mode_t aaudio_sharing_mode_t

    /**
     * This will be the only stream using a particular source or sink.
     * This mode will provide the lowest possible latency.
     * You should close EXCLUSIVE streams immediately when you are not using them.
     */
#define OBOE_SHARING_MODE_EXCLUSIVE   AAUDIO_SHARING_MODE_EXCLUSIVE
    /**
     * Multiple applications will be mixed by the Oboe Server.
     * This will have higher latency than the EXCLUSIVE mode.
     */
#define OBOE_SHARING_MODE_SHARED      AAUDIO_SHARING_MODE_SHARED

    /**
     * No particular performance needs. Default.
     */
#define OBOE_PERFORMANCE_MODE_NONE AAUDIO_PERFORMANCE_MODE_NONE

    /**
     * Extending battery life is most important.
     */
#define OBOE_PERFORMANCE_MODE_POWER_SAVING AAUDIO_PERFORMANCE_MODE_POWER_SAVING

    /**
     * Reducing latency is most important.
     */
#define OBOE_PERFORMANCE_MODE_LOW_LATENCY  AAUDIO_PERFORMANCE_MODE_LOW_LATENCY

#define oboe_performance_mode_t  aaudio_performance_mode_t

#ifdef __cplusplus
}
#endif

#endif // OBOE_OBOE_DEFINITIONS_H
