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

#ifndef RHYTHMGAME_TIMEFUNCTIONS_H
#define RHYTHMGAME_TIMEFUNCTIONS_H

#include <stdint.h>


constexpr int kMillisecondsInSecond = 1000;
constexpr int kMicrosecondsInMillisecond = 1000;
constexpr int kNanosecondsInMillisecond = 1000000;
constexpr int kSecondsInMinute = 60;
constexpr int kBeatsInBar = 4; // 4/4 time
constexpr int kSemiQuaversPerBar = 16;
constexpr int kWindowCenterOffset = 100;

enum class TapResult {
    Early,
    Late,
    Success
};


int64_t nowUptimeMillis();

int64_t nowEpochMillis();

int64_t convertUptimeToEpoch(long eventTime);

template <typename FromType>
const char * convertToText2(FromType);

int64_t convertBeatToFrameNumber(const int barNumber,
                                 const int semiQuaverNumber,
                                 const int tempoBpm,
                                 const int sampleRate);

constexpr int64_t convertMillisToFrames(const long millis, const int sampleRate) {
    return millis * (sampleRate / kMillisecondsInSecond);
}

constexpr int64_t convertFramesToMillis(const int64_t frames, const int sampleRate){
    return (int64_t)(((double)frames / sampleRate) * kMillisecondsInSecond);
}


TapResult getTapResult(int64_t tapTimeInMillis, int64_t tapWindowInMillis);

void renderEvent(TapResult r);


#endif //RHYTHMGAME_TIMEFUNCTIONS_H
