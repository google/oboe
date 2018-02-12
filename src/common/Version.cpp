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

#include <sstream>
#include "oboe/Version.h"

namespace oboe {

// Max digits in 32-bit unsigned int = 10, plus two periods, plus null terminator = 13
constexpr int kMaxVersionStringLength = 13;

const char * Version::toString() {

    static char text[kMaxVersionStringLength];
    snprintf(text, kMaxVersionStringLength, "%d.%d.%d",
             MajorNumber,
             MinorNumber,
             SubMinorNumber);
    return text;
}

uint32_t Version::toInt(){
    return MajorNumber << 24 | MinorNumber << 16 | SubMinorNumber;
}

} // namespace oboe