/*
 * Copyright 2018 The Android Open Source Project
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

#include "audio_common.h"
#include <logging_macros.h>
#include <string>
#include <cinttypes>

uint16_t SampleFormatToBpp(oboe::AudioFormat format) {
    switch (format) {
      case oboe::AudioFormat::I16:
        return 16;
      case oboe::AudioFormat::Float:
        return 32;
      default:
        return 0;
    }
}

void PrintAudioStreamInfo(const oboe::AudioStream *stream) {
  LOGI("StreamID: %p", stream);
  oboe::Direction  dir = stream->getDirection();
  LOGI("Direction: %s", oboe::convertToText(dir));
  LOGI("API type: %s", oboe::convertToText(stream->getAudioApi()));
  LOGI("BufferCapacity: %d", stream->getBufferCapacityInFrames());
  LOGI("BufferSize: %d", stream->getBufferSizeInFrames());
  LOGI("FramesPerBurst: %d", stream->getFramesPerBurst());
  LOGI("XRunCount: %d", stream->getXRunCount().value());
  LOGI("SampleRate: %d", stream->getSampleRate());
  LOGI("SamplesPerFrame: %d", stream->getChannelCount());
  LOGI("DeviceId: %d", stream->getDeviceId());
  LOGI("Format: %s",  oboe::convertToText(stream->getFormat()));
  LOGI("SharingMode: %s", oboe::convertToText(stream->getSharingMode()));
  LOGI("PerformanceMode: %s", oboe::convertToText(stream->getPerformanceMode()));

  if (dir == oboe::Direction ::Output) {
      LOGI("FramesReadByDevice: %" PRIx64, stream->getFramesRead());
      LOGI("FramesWriteByApp: %d", (int32_t)stream->getFramesWritten());
  } else {
      LOGI("FramesReadByApp: %d", (int32_t)stream->getFramesRead());
      LOGI("FramesWriteByDevice: %d", (int32_t)stream->getFramesWritten());
  }
}

int64_t timestamp_to_nanoseconds(timespec ts) {
  return (ts.tv_sec * oboe::kNanosPerSecond) + ts.tv_nsec;
}

int64_t get_time_nanoseconds(clockid_t clockid) {
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