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
