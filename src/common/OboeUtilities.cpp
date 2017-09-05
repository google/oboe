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
#include <sys/types.h>
#include "oboe/OboeDefinitions.h"
#include "common/OboeUtilities.h"

void OboeConvert_floatToPcm16(const float *source, int16_t *destination, int32_t numSamples) {
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

void OboeConvert_pcm16ToFloat(const int16_t *source, float *destination, int32_t numSamples) {
    for (int i = 0; i < numSamples; i++) {
        destination[i] = source[i] * (1.0f / 32768.0f);
    }
}

int32_t OboeConvert_formatToSizeInBytes(oboe_audio_format_t format) {
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
