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

#ifndef OBOE_UTILITIES_H
#define OBOE_UTILITIES_H

#include <unistd.h>
#include <sys/types.h>
#include "oboe/Definitions.h"

namespace oboe {

void convertFloatToPcm16(const float *source, int16_t *destination, int32_t numSamples);
void convertPcm16ToFloat(const int16_t *source, float *destination, int32_t numSamples);

/**
 * @return the size of a sample of the given format in bytes or OBOE_ERROR_ILLEGAL_ARGUMENT
 */
int32_t convertFormatToSizeInBytes(AudioFormat format);

/**
 * The text is the ASCII symbol corresponding to the supplied Oboe enum value,
 * or an English message saying the value is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @param enum value @see common/Utilities.cpp for concrete implementations
 * @return text representation of an Oboe enum value.
 */
template <typename FromType>
const char * convertToText(FromType);

/**
 * Return the version of the SDK that is currently running.
 *
 * For example, on Android, this would return 27 for Oreo 8.1.
 * If the version number cannot be determined then this will return -1.
 *
 * @return version number or -1
 */
int getSdkVersion();

} // namespace oboe

#endif //OBOE_UTILITIES_H
