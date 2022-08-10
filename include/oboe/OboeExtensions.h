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

#ifndef OBOE_EXTENSIONS_
#define OBOE_EXTENSIONS_

#include <stdint.h>

#include "oboe/Definitions.h"
#include "oboe/AudioStream.h"

namespace oboe {

/**
 * Extra Oboe APIs to query and set whether MMAP is enabled.
 */
class OboeExtensions {
public:
    static bool isMMapSupported();

    static bool isMMapEnabled();

    /**
     * Controls whether the MMAP data path can be selected when opening a stream.
     * It has no effect after the stream has been opened.
     * It only affects the application that calls it. Other apps are not affected.
     *
     * @param enabled
     * @return 0 or a negative error code
     */
    static int32_t setMMapEnabled(bool enabled);

    static bool isMMapUsed(oboe::AudioStream *oboeStream);
};

} // namespace oboe

#endif // OBOE_LATENCY_TUNER_
