/*
 * Copyright 2016 The Android Open Source Project
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


#include <unistd.h>
#include "oboe/OboeDefinitions.h"
#include "oboe/OboeUtilities.h"

#define OBOE_CASE_ENUM(name) case name: return #name

void Oboe_convertFloatToPcm16(const float *source, int16_t *destination, int32_t numSamples) {
    for (int i = 0; i < numSamples; i++) {
        float fval = source[i];
        fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
        fval *= 32768.0f;
        int32_t sample = (int32_t) fval;
        // clip to 16-bit range
        if (sample < 0) sample = 0;
        else if (sample > 0x0FFFF) sample = 0x0FFFF;
        sample -= 32768; // center at zero
        destination[i] = (int16_t) sample;
    }
}

void Oboe_convertPcm16ToFloat(const int16_t *source, float *destination, int32_t numSamples) {
    for (int i = 0; i < numSamples; i++) {
        destination[i] = source[i] * (1.0f / 32768.0f);
    }
}

int32_t Oboe_convertFormatToSizeInBytes(oboe_audio_format_t format) {
    int32_t size = OBOE_ERROR_ILLEGAL_ARGUMENT;
    switch (format) {
        case OBOE_AUDIO_FORMAT_PCM_I16:
            size = sizeof(int16_t);
            break;
        case OBOE_AUDIO_FORMAT_PCM_FLOAT:
            size = sizeof(float);
            break;
        default:
            break;
    }
    return size;
}

const char *Oboe_convertResultToText(oboe_result_t returnCode) {
    switch (returnCode) {
        OBOE_CASE_ENUM(OBOE_OK);
        OBOE_CASE_ENUM(OBOE_ERROR_DISCONNECTED);
        OBOE_CASE_ENUM(OBOE_ERROR_ILLEGAL_ARGUMENT);
            // reserved
        OBOE_CASE_ENUM(OBOE_ERROR_INTERNAL);
        OBOE_CASE_ENUM(OBOE_ERROR_INVALID_STATE);
            // reserved
            // reserved
        OBOE_CASE_ENUM(OBOE_ERROR_INVALID_HANDLE);
            // reserved
        OBOE_CASE_ENUM(OBOE_ERROR_UNIMPLEMENTED);
        OBOE_CASE_ENUM(OBOE_ERROR_UNAVAILABLE);
        OBOE_CASE_ENUM(OBOE_ERROR_NO_FREE_HANDLES);
        OBOE_CASE_ENUM(OBOE_ERROR_NO_MEMORY);
        OBOE_CASE_ENUM(OBOE_ERROR_NULL);
        OBOE_CASE_ENUM(OBOE_ERROR_TIMEOUT);
        OBOE_CASE_ENUM(OBOE_ERROR_WOULD_BLOCK);
        OBOE_CASE_ENUM(OBOE_ERROR_INVALID_FORMAT);
        OBOE_CASE_ENUM(OBOE_ERROR_OUT_OF_RANGE);
        OBOE_CASE_ENUM(OBOE_ERROR_NO_SERVICE);
        OBOE_CASE_ENUM(OBOE_ERROR_INVALID_RATE);
    }
    return "Unrecognized Oboe error.";
}

const char *Oboe_convertAudioFormatToText(oboe_audio_format_t format) {
    switch (format) {
        OBOE_CASE_ENUM(OBOE_AUDIO_FORMAT_INVALID);
        OBOE_CASE_ENUM(OBOE_AUDIO_FORMAT_UNSPECIFIED);
        OBOE_CASE_ENUM(OBOE_AUDIO_FORMAT_PCM_I16);
        OBOE_CASE_ENUM(OBOE_AUDIO_FORMAT_PCM_FLOAT);
    }
    return "Unrecognized audio format.";
}

const char *Oboe_convertPerformanceModeToText(oboe_performance_mode_t mode) {
    switch (mode) {
        OBOE_CASE_ENUM(OBOE_PERFORMANCE_MODE_LOW_LATENCY);
        OBOE_CASE_ENUM(OBOE_PERFORMANCE_MODE_NONE);
        OBOE_CASE_ENUM(OBOE_PERFORMANCE_MODE_POWER_SAVING);
    }
    return "Unrecognised performance mode.";
}

const char *Oboe_convertSharingModeToText(oboe_sharing_mode_t mode) {
    switch (mode) {
        OBOE_CASE_ENUM(OBOE_SHARING_MODE_EXCLUSIVE);
        OBOE_CASE_ENUM(OBOE_SHARING_MODE_SHARED);
    }
    return "Unrecognised sharing mode.";
}

const char *Oboe_convertDataCallbackResultToText(oboe_data_callback_result_t result) {
    switch (result) {
        OBOE_CASE_ENUM(OBOE_CALLBACK_RESULT_CONTINUE);
        OBOE_CASE_ENUM(OBOE_CALLBACK_RESULT_STOP);
    }
    return "Unrecognised data callback result.";
}

const char *Oboe_convertDirectionToText(oboe_direction_t direction) {
    switch (direction) {
        OBOE_CASE_ENUM(OBOE_DIRECTION_INPUT);
        OBOE_CASE_ENUM(OBOE_DIRECTION_OUTPUT);
    }
    return "Unrecognised direction.";
}
