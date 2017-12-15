/*
 * Copyright 2017 The Android Open Source Project
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

#include "OpenSLESUtilities.h"

namespace oboe {

/*
 * OSLES Helpers
 */
static const char *errStrings[] = {
        "SL_RESULT_SUCCESS",                  // 0
        "SL_RESULT_PRECONDITIONS_VIOLATE",    // 1
        "SL_RESULT_PARAMETER_INVALID",        // 2
        "SL_RESULT_MEMORY_FAILURE",           // 3
        "SL_RESULT_RESOURCE_ERROR",           // 4
        "SL_RESULT_RESOURCE_LOST",            // 5
        "SL_RESULT_IO_ERROR",                 // 6
        "SL_RESULT_BUFFER_INSUFFICIENT",      // 7
        "SL_RESULT_CONTENT_CORRUPTED",        // 8
        "SL_RESULT_CONTENT_UNSUPPORTED",      // 9
        "SL_RESULT_CONTENT_NOT_FOUND",        // 10
        "SL_RESULT_PERMISSION_DENIED",        // 11
        "SL_RESULT_FEATURE_UNSUPPORTED",      // 12
        "SL_RESULT_INTERNAL_ERROR",           // 13
        "SL_RESULT_UNKNOWN_ERROR",            // 14
        "SL_RESULT_OPERATION_ABORTED",        // 15
        "SL_RESULT_CONTROL_LOST"              // 16
};

const char *getSLErrStr(SLresult code) {
    return errStrings[code];
}

SLAndroidDataFormat_PCM_EX OpenSLES_createExtendedFormat(
        SLDataFormat_PCM format, SLuint32 representation) {
    SLAndroidDataFormat_PCM_EX format_pcm_ex;
    format_pcm_ex.formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
    format_pcm_ex.numChannels = format.numChannels;
    format_pcm_ex.sampleRate = format.samplesPerSec;
    format_pcm_ex.bitsPerSample = format.bitsPerSample;
    format_pcm_ex.containerSize = format.containerSize;
    format_pcm_ex.channelMask = format.channelMask;
    format_pcm_ex.endianness = format.endianness;
    format_pcm_ex.representation = representation;
    return format_pcm_ex;
}

SLuint32 OpenSLES_ConvertFormatToRepresentation(AudioFormat format) {
    switch(format) {
        case AudioFormat::I16:
            return SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;
        case AudioFormat::Float:
            return SL_ANDROID_PCM_REPRESENTATION_FLOAT;
        case AudioFormat::Invalid:
        case AudioFormat::Unspecified:
        default:
            return 0;
    }
}

} // namespace oboe