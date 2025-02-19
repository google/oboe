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

#ifndef OBOE_TRACE_H
#define OBOE_TRACE_H

#include <cstdint>

namespace oboe {

/**
 * Wrapper for tracing use with Perfetto
 */
class Trace {

public:
    static void beginSection(const char *format, ...);

    static void endSection();

    static void setCounter(const char *counterName, int64_t counterValue);

    static void initialize();

private:
    static bool mIsTracingEnabled;
    static bool mIsSetCounterSupported;
    static bool mHasErrorBeenShown;
};

}
#endif //OBOE_TRACE_H
