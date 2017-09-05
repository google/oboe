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

#ifndef OBOE_OBOE_UTILITIES_H
#define OBOE_OBOE_UTILITIES_H

#include <unistd.h>
#include <sys/types.h>
#include "oboe/OboeDefinitions.h"

void OboeConvert_floatToPcm16(const float *source, int16_t *destination, int32_t numSamples);
void OboeConvert_pcm16ToFloat(const int16_t *source, float *destination, int32_t numSamples);

/**
 * @return the size of a sample of the given format in bytes or OBOE_ERROR_ILLEGAL_ARGUMENT
 */
int32_t OboeConvert_formatToSizeInBytes(oboe_audio_format_t format);

#endif //OBOE_OBOE_UTILITIES_H
