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

#ifndef SAMPLES_GAMECONSTANTS_H
#define SAMPLES_GAMECONSTANTS_H

#include "ui/OpenGLFunctions.h"

constexpr int kSampleRateHz = 48000; // Fixed sample rate, see README
constexpr int kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)
constexpr int kMaxQueueItems = 4; // Must be power of 2

constexpr ScreenColor kScreenBackgroundColor = GREY;
constexpr ScreenColor kTapSuccessColor = GREEN;
constexpr ScreenColor kTapEarlyColor = ORANGE;
constexpr ScreenColor kTapLateColor = PURPLE;

// This defines the size of the tap window in milliseconds. For example, if defined at 100ms the
// player will have 100ms before and after the centre of the tap window to tap on the screen and
// be successful
constexpr int kWindowCenterOffsetMs = 100;

#endif //SAMPLES_GAMECONSTANTS_H
