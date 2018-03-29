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

#include <time.h>
#include <chrono>
#include <ui/OpenGLFunctions.h>
#include "UtilityFunctions.h"
#include "logging.h"

int64_t nowUptimeMillis() {
    struct timespec res;
    clock_gettime(CLOCK_MONOTONIC, &res);
    return (res.tv_sec * kMillisecondsInSecond) + res.tv_nsec / kNanosecondsInMillisecond;
}

int64_t nowEpochMillis() {
    using namespace std::chrono;
    return time_point_cast<milliseconds>(high_resolution_clock::now()).time_since_epoch().count();
}

int64_t convertUptimeToEpoch(long eventTime) {
    auto deltaElapsedTime = nowUptimeMillis() - eventTime;
    return nowEpochMillis() - deltaElapsedTime;
}

int64_t convertBeatToFrameNumber(const int barNumber,
                                 const int semiQuaverNumber,
                                 const int tempoBpm,
                                 const int sampleRate) {

    int64_t framesInMinute = sampleRate * kSecondsInMinute;
    float framesPerBeat = framesInMinute / tempoBpm;
    float framesPerBar = framesPerBeat * kBeatsInBar;
    float framesPerSemiQuaver = framesPerBar / kSemiQuaversPerBar;

    return (int64_t)((barNumber * framesPerBar) + (semiQuaverNumber * framesPerSemiQuaver));
}

TapResult getTapResult(int64_t tapTimeInMillis, int64_t tapWindowInMillis){

    if (tapTimeInMillis <= tapWindowInMillis + kWindowCenterOffset) {
        if (tapTimeInMillis >= tapWindowInMillis - kWindowCenterOffset) {
            return TapResult::Success;
        } else {
            return TapResult::Early;
        }
    } else {
        return TapResult::Late;
    }
}


void renderEvent(TapResult r){

    switch (r) {
        case TapResult::Success:
            SetGLScreenColor(GREEN);
            break;
        case TapResult::Early:
            SetGLScreenColor(ORANGE);
            break;
        case TapResult::Late:
            SetGLScreenColor(PURPLE);
            break;
    }
}