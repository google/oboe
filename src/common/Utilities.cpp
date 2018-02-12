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
#include "oboe/Definitions.h"
#include "oboe/Utilities.h"

#define OBOE_CASE_ENUM(name) case name: return #name

namespace oboe {

void convertFloatToPcm16(const float *source, int16_t *destination, int32_t numSamples) {
    for (int i = 0; i < numSamples; i++) {
        float fval = source[i];
        fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
        fval *= 32768.0f;
        auto sample = (int32_t) fval;
        // clip to 16-bit range
        if (sample < 0) sample = 0;
        else if (sample > 0x0FFFF) sample = 0x0FFFF;
        sample -= 32768; // center at zero
        destination[i] = (int16_t) sample;
    }
}

void convertPcm16ToFloat(const int16_t *source, float *destination, int32_t numSamples) {
    for (int i = 0; i < numSamples; i++) {
        destination[i] = source[i] * (1.0f / 32768.0f);
    }
}

int32_t convertFormatToSizeInBytes(AudioFormat format) {
    int32_t size = 0;
    switch (format) {
        case AudioFormat::I16:
            size = sizeof(int16_t);
            break;
        case AudioFormat::Float:
            size = sizeof(float);
            break;
        default:
            break;
    }
    return size;
}

const char *convertResultToText(Result returnCode) {
    switch (returnCode) {
        OBOE_CASE_ENUM(Result::OK);
        OBOE_CASE_ENUM(Result::ErrorBase);
        OBOE_CASE_ENUM(Result::ErrorDisconnected);
        OBOE_CASE_ENUM(Result::ErrorIllegalArgument);
        OBOE_CASE_ENUM(Result::ErrorInternal);
        OBOE_CASE_ENUM(Result::ErrorInvalidState);
        OBOE_CASE_ENUM(Result::ErrorInvalidHandle);
        OBOE_CASE_ENUM(Result::ErrorUnimplemented);
        OBOE_CASE_ENUM(Result::ErrorUnavailable);
        OBOE_CASE_ENUM(Result::ErrorNoFreeHandles);
        OBOE_CASE_ENUM(Result::ErrorNoMemory);
        OBOE_CASE_ENUM(Result::ErrorNull);
        OBOE_CASE_ENUM(Result::ErrorTimeout);
        OBOE_CASE_ENUM(Result::ErrorWouldBlock);
        OBOE_CASE_ENUM(Result::ErrorInvalidFormat);
        OBOE_CASE_ENUM(Result::ErrorOutOfRange);
        OBOE_CASE_ENUM(Result::ErrorNoService);
        OBOE_CASE_ENUM(Result::ErrorInvalidRate);
    }
}

const char *convertAudioFormatToText(AudioFormat format) {
    switch (format) {
        OBOE_CASE_ENUM(AudioFormat::Invalid);
        OBOE_CASE_ENUM(AudioFormat::Unspecified);
        OBOE_CASE_ENUM(AudioFormat::I16);
        OBOE_CASE_ENUM(AudioFormat::Float);
    }
}

const char *convertPerformanceModeToText(PerformanceMode mode) {
    switch (mode) {
        OBOE_CASE_ENUM(PerformanceMode::LowLatency);
        OBOE_CASE_ENUM(PerformanceMode::None);
        OBOE_CASE_ENUM(PerformanceMode::PowerSaving);
    }
}

const char *convertSharingModeToText(SharingMode mode) {
    switch (mode) {
        OBOE_CASE_ENUM(SharingMode::Exclusive);
        OBOE_CASE_ENUM(SharingMode::Shared);
    }
}

const char *convertDataCallbackResultToText(DataCallbackResult result) {
    switch (result) {
        OBOE_CASE_ENUM(DataCallbackResult::Continue);
        OBOE_CASE_ENUM(DataCallbackResult::Stop);
    }
}

const char *convertDirectionToText(Direction direction) {
    switch (direction) {
        OBOE_CASE_ENUM(Direction::Input);
        OBOE_CASE_ENUM(Direction::Output);
    }
}

} // namespace oboe