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

#include <string>
#include "audio_common.h"
#include <logging_macros.h>

static const oboe::AudioFormat audioFormatEnum[] = {
    oboe::AudioFormat::Invalid,
    oboe::AudioFormat::Unspecified,
    oboe::AudioFormat::I16,
    oboe::AudioFormat::Float,
};
static const int32_t audioFormatCount = sizeof(audioFormatEnum)/
                                        sizeof(audioFormatEnum[0]);

static const uint32_t sampleFormatBPP[] = {
    0xffff,
    0xffff,
    16, //I16
    32, //FLOAT
};
uint16_t SampleFormatToBpp(oboe::AudioFormat format) {
    for (int32_t i = 0; i < audioFormatCount; ++i) {
      if (audioFormatEnum[i] == format)
        return sampleFormatBPP[i];
    }
    return 0xffff;
}
static const char * audioFormatStr[] = {
    "AAUDIO_FORMAT_INVALID", // = -1,
    "AAUDIO_FORMAT_UNSPECIFIED", // = 0,
    "AAUDIO_FORMAT_PCM_I16",
    "AAUDIO_FORMAT_PCM_FLOAT",
};
const char* FormatToString(oboe::AudioFormat format) {
    for (int32_t i = 0; i < audioFormatCount; ++i) {
        if (audioFormatEnum[i] == format)
            return audioFormatStr[i];
    }
    return "UNKNOW_AUDIO_FORMAT";
}

void PrintAudioStreamInfo(const oboe::AudioStream *stream) {
#define STREAM_CALL(func) (stream)->func()
    LOGI("StreamID: %p", stream);

    LOGI("API type: %s", oboe::convertToText(stream->getAudioApi()));
    LOGI("BufferCapacity: %d", STREAM_CALL(getBufferCapacityInFrames));
    LOGI("BufferSize: %d", STREAM_CALL(getBufferSizeInFrames));
//  Question: does this one have to non-constant function?
//    LOGI("FramesPerBurst: %d", STREAM_CALL(getFramesPerBurst));
    LOGI("FramesPerBurst: %d", const_cast<oboe::AudioStream* >(stream)->getFramesPerBurst());
    LOGI("XRunCount: %d", STREAM_CALL(getXRunCount));
    LOGI("SampleRate: %d", STREAM_CALL(getSampleRate));
    LOGI("SamplesPerFrame: %d", STREAM_CALL(getChannelCount));
    LOGI("DeviceId: %d", STREAM_CALL(getDeviceId));
    LOGI("Format: %s",  FormatToString(STREAM_CALL(getFormat)));
    LOGI("SharingMode: %s", (STREAM_CALL(getSharingMode)) == oboe::SharingMode::Exclusive?
                          "EXCLUSIVE" : "SHARED");

    oboe::PerformanceMode perfMode = STREAM_CALL(getPerformanceMode);
    std::string perfModeDescription;
    switch (perfMode){
      case oboe::PerformanceMode ::None:
        perfModeDescription = "NONE";
        break;
      case oboe::PerformanceMode::LowLatency:
        perfModeDescription = "LOW_LATENCY";
        break;
      case oboe::PerformanceMode::PowerSaving:
        perfModeDescription = "POWER_SAVING";
        break;
      default:
        perfModeDescription = "UNKNOWN";
        break;
    }
    LOGI("PerformanceMode: %s", perfModeDescription.c_str());

    oboe::Direction  dir = STREAM_CALL(getDirection);
    LOGI("Direction: %s", (dir == oboe::Direction ::Output ? "OUTPUT" : "INPUT"));
    if (dir == oboe::Direction ::Output) {
        LOGI("FramesReadByDevice: %d", (int32_t)STREAM_CALL(getFramesRead));
        LOGI("FramesWriteByApp: %d", (int32_t)STREAM_CALL(getFramesWritten));
    } else {
        LOGI("FramesReadByApp: %d", (int32_t)STREAM_CALL(getFramesRead));
        LOGI("FramesWriteByDevice: %d", (int32_t)STREAM_CALL(getFramesWritten));
    }
#undef STREAM_CALL
}

int64_t timestamp_to_nanoseconds(timespec ts){
  return (ts.tv_sec * (int64_t) NANOS_PER_SECOND) + ts.tv_nsec;
}

int64_t get_time_nanoseconds(clockid_t clockid){
  timespec ts;
  clock_gettime(clockid, &ts);
  return timestamp_to_nanoseconds(ts);
}

void ConvertMonoToStereo(int16_t *buffer, int32_t numFrames) {
    for (int i = numFrames - 1; i >= 0; i--) {
        buffer[i*2] = buffer[i];
        buffer[(i*2)+1] = buffer[i];
    }
}