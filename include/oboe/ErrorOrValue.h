/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef OBOE_ERROR_OR_VALUE_H
#define OBOE_ERROR_OR_VALUE_H

#include "oboe/Definitions.h"

namespace oboe {

template <typename T>
class ErrorOrValue {
public:
    explicit ErrorOrValue(oboe::Result error)
            : mValue{}
            , mError(error) {}

    explicit ErrorOrValue(T value)
            : mValue(value)
            , mError(oboe::Result::OK) {}

    oboe::Result error() const {
        return mError;
    }

    T value() const {
        return mValue;
    }

    /**
     * @return true if OK
     */
    explicit operator bool() const { return mError == oboe::Result::OK; }

    /**
     * Quick way to check for an error.
     *
     * The caller could write something like this:
     * <code>
     *     if (!result) { printf("Got error %s\n", convertToText(result.error())); }
     * </code>
     *
     * @return true if an error occurred
     */
    bool operator !() const { return mError != oboe::Result::OK; }

private:
    const T             mValue;
    const oboe::Result  mError;
};

} // namespace oboe

#endif //OBOE_ERROR_OR_VALUE_H
