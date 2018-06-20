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

#ifndef OBOE_RESULT_WITH_VALUE_H
#define OBOE_RESULT_WITH_VALUE_H

#include "oboe/Definitions.h"

namespace oboe {

template <typename T>
class ResultWithValue {
public:
    ResultWithValue(oboe::Result error)
            : mValue{}
            , mError(error) {}

    explicit ResultWithValue(T value)
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

    /**
     * Implicitly convert to a Result. This enables easy comparison with Result values. Example:
     *
     * <code>
     *     ResultWithValue result = openStream();
     *     if (result == Result::ErrorNoMemory){ // tell user they're out of memory }
     * </code>
     */
    operator Result() const {
        return mError;
    }

    /**
     * Create a ResultWithValue from a number. If the number is positive the ResultWithValue will
     * have a result of Result::OK and the value will contain the number. If the number is negative
     * the result will be obtained from the negative number (numeric error codes can be found in
     * AAudio.h) and the value will be null.
     *
     */
    static ResultWithValue<T> createBasedOnSign(T numericResult){

        // Ensure that the type is either an integer or float
        static_assert(std::is_arithmetic<T>::value,
                      "createBasedOnSign can only be called for numeric types (int or float)");

        if (numericResult >= 0){
            return ResultWithValue<T>(numericResult);
        } else {
            return ResultWithValue<T>(static_cast<Result>(numericResult));
        }
    }


private:
    const T             mValue;
    const oboe::Result  mError;
};

} // namespace oboe

#endif //OBOE_RESULT_WITH_VALUE_H
