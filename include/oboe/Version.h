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

#ifndef OBOE_VERSIONINFO_H
#define OBOE_VERSIONINFO_H

namespace oboe {

class Version {

public:
    // This is incremented when we make breaking API changes. Based loosely on https://semver.org/
    static constexpr uint8_t MajorNumber = 0;

    // This is incremented when we add backwards compatible functionality. Or set to zero when kVersionMajor is
    // incremented
    static constexpr uint8_t MinorNumber = 9;

    // This is incremented when we make backwards compatible bug fixes. Or set to zero when kVersionMinor is
    // incremented
    static constexpr uint16_t SubMinorNumber = 0;

    /**
     * Provides a text representation of the current Oboe library version in the form:
     *
     * MAJOR.MINOR.SUBMINOR
     *
     * @return A string containing the current Oboe library version
     */
    static const char * toString();

    /**
     * Provides an integer representation of the current Oboe library version. This will always increase when the
     * version number changes so can be compared using integer comparison.
     *
     * @return an integer representing the current Oboe library version.
     */
    static uint32_t toInt();
};

} // namespace oboe
#endif //OBOE_VERSIONINFO_H
