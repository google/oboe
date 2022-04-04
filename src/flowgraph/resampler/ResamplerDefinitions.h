/*
 * Copyright 2022 The Android Open Source Project
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

// Set flags RESAMPLER_ANDROID_INTERNAL and RESAMPLER_OUTER_NAMESPACE based on whether compiler
// flag USE_FLOWGRAPH_ANDROID_INTERNAL is defined. USE_FLOWGRAPH_ANDROID_INTERNAL should be defined
// in aaudio but not in oboe.

#ifndef RESAMPLER_ANDROID_INTERNAL
#ifdef USE_FLOWGRAPH_ANDROID_INTERNAL
#define RESAMPLER_ANDROID_INTERNAL 1
#else
#define RESAMPLER_ANDROID_INTERNAL 0
#endif // USE_FLOWGRAPH_ANDROID_INTERNAL
#endif // RESAMPLER_ANDROID_INTERNAL

#ifndef RESAMPLER_OUTER_NAMESPACE
#ifdef USE_FLOWGRAPH_ANDROID_INTERNAL
#define RESAMPLER_OUTER_NAMESPACE aaudio
#else
#define RESAMPLER_OUTER_NAMESPACE oboe
#endif // USE_FLOWGRAPH_ANDROID_INTERNAL
#endif // RESAMPLER_OUTER_NAMESPACE
