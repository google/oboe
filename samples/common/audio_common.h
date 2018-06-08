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


#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

#include <chrono>
#include <oboe/Oboe.h>


constexpr int kMonoChannelCount = 1;
constexpr int kStereoChannelCount = 2;

uint16_t SampleFormatToBpp(oboe::AudioFormat format);
/*
 * GetSystemTicks(void):  return the time in micro sec
 */
__inline__ uint64_t GetSystemTicks(void) {
    struct timeval Time;
    gettimeofday( &Time, NULL );

    return (static_cast<uint64_t>(oboe::kNanosPerMillisecond) * Time.tv_sec + Time.tv_usec);
}

/*
 * flag to enable file dumping
 */
// #define ENABLE_LOG  1

void PrintAudioStreamInfo(const oboe::AudioStream *stream);

int64_t timestamp_to_nanoseconds(timespec ts);

int64_t get_time_nanoseconds(clockid_t clockid);

// Note: buffer must be at least double the length of numFrames to accommodate the stereo data
void ConvertMonoToStereo(int16_t *buffer, int32_t numFrames);

#endif // AUDIO_COMMON_H
