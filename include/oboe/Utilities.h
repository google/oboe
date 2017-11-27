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
 * The text is the ASCII symbol corresponding to the returnCode,
 * or an English message saying the returnCode is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @return pointer to a text representation of an Oboe result code.
 */
const char * convertResultToText(Result returnCode);

/**
 * The text is the ASCII symbol corresponding to the audio format,
 * or an English message saying the audio format is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @return pointer to a text representation of an Oboe audio format.
 */
const char * convertAudioFormatToText(AudioFormat format);

/**
 * The text is the ASCII symbol corresponding to the performance mode,
 * or an English message saying the performance is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @return pointer to a text representation of an Oboe performance mode.
 */
const char * convertPerformanceModeToText(PerformanceMode mode);

/**
 * The text is the ASCII symbol corresponding to the sharing mode,
 * or an English message saying the sharing mode is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @return pointer to a text representation of an Oboe sharing mode.
 */
const char * convertSharingModeToText(SharingMode mode);

/**
 * The text is the ASCII symbol corresponding to the data callback result,
 * or an English message saying the data callback result is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @return pointer to a text representation of an Oboe data callback result.
 */
const char * convertDataCallbackResultToText(DataCallbackResult result);

/**
 * The text is the ASCII symbol corresponding to the stream direction,
 * or an English message saying the stream direction is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for displaying to users.
 *
 * @return pointer to a text representation of an Oboe stream direction.
 */
const char * convertDirectionToText(Direction direction);

} // namespace oboe

#endif //OBOE_UTILITIES_H
