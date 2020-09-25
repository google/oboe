/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef OBOETESTER_AAUDIO_EXTENSIONS_H
#define OBOETESTER_AAUDIO_EXTENSIONS_H

#include <dlfcn.h>
#include <stdint.h>

#define LIB_AAUDIO_NAME          "libaaudio.so"
#define FUNCTION_IS_MMAP         "AAudioStream_isMMapUsed"
#define FUNCTION_SET_MMAP_POLICY "AAudio_setMMapPolicy"
#define FUNCTION_GET_MMAP_POLICY "AAudio_getMMapPolicy"

typedef struct AAudioStreamStruct         AAudioStream;

/**
 * Call some AAudio test routines that are not part of the normal API.
 */
class AAudioExtensions {
public:
    AAudioExtensions() {
        int32_t policy = getIntegerProperty("aaudio.mmap_policy", 0);
        mMMapSupported = isPolicyEnabled(policy);

        policy = getIntegerProperty("aaudio.mmap_exclusive_policy", 0);
        mMMapExclusiveSupported = isPolicyEnabled(policy);
    }

    static bool isPolicyEnabled(int32_t policy) {
        return (policy == AAUDIO_POLICY_AUTO || policy == AAUDIO_POLICY_ALWAYS);
    }

    static AAudioExtensions &getInstance() {
        static AAudioExtensions instance;
        return instance;
    }

    bool isMMapUsed(oboe::AudioStream *oboeStream) {
        if (!loadLibrary()) return false;
        if (mAAudioStream_isMMap == nullptr) return false;
        AAudioStream *aaudioStream = (AAudioStream *) oboeStream->getUnderlyingStream();
        return mAAudioStream_isMMap(aaudioStream);
    }

    bool setMMapEnabled(bool enabled) {
        if (!loadLibrary()) return false;
        if (mAAudio_setMMapPolicy == nullptr) return false;
        return mAAudio_setMMapPolicy(enabled ? AAUDIO_POLICY_AUTO : AAUDIO_POLICY_NEVER);
    }

    bool isMMapEnabled() {
        if (!loadLibrary()) return false;
        if (mAAudio_getMMapPolicy == nullptr) return false;
        int32_t policy = mAAudio_getMMapPolicy();
        return isPolicyEnabled(policy);
    }

    bool isMMapSupported() {
        return mMMapSupported;
    }

    bool isMMapExclusiveSupported() {
        return mMMapExclusiveSupported;
    }

private:

    enum {
        AAUDIO_POLICY_NEVER = 1,
        AAUDIO_POLICY_AUTO,
        AAUDIO_POLICY_ALWAYS
    };
    typedef int32_t aaudio_policy_t;

    int getIntegerProperty(const char *name, int defaultValue) {
        int result = defaultValue;
        char valueText[PROP_VALUE_MAX] = {0};
        if (__system_property_get(name, valueText) != 0) {
            result = atoi(valueText);
        }
        return result;
    }

    // return true if it succeeds
    bool loadLibrary() {
        if (mFirstTime) {
            mFirstTime = false;
            mLibHandle = dlopen(LIB_AAUDIO_NAME, 0);
            if (mLibHandle == nullptr) {
                LOGI("%s() could not find " LIB_AAUDIO_NAME, __func__);
                return false;
            }

            mAAudioStream_isMMap = (bool (*)(AAudioStream *stream))
                    dlsym(mLibHandle, FUNCTION_IS_MMAP);
            if (mAAudioStream_isMMap == nullptr) {
                LOGI("%s() could not find " FUNCTION_IS_MMAP, __func__);
                return false;
            }

            mAAudio_setMMapPolicy = (int32_t (*)(aaudio_policy_t policy))
                    dlsym(mLibHandle, FUNCTION_SET_MMAP_POLICY);
            if (mAAudio_setMMapPolicy == nullptr) {
                LOGI("%s() could not find " FUNCTION_SET_MMAP_POLICY, __func__);
                return false;
            }

            mAAudio_getMMapPolicy = (aaudio_policy_t (*)())
                    dlsym(mLibHandle, FUNCTION_GET_MMAP_POLICY);
            if (mAAudio_getMMapPolicy == nullptr) {
                LOGI("%s() could not find " FUNCTION_GET_MMAP_POLICY, __func__);
                return false;
            }
        }
        return (mLibHandle != nullptr);
    }

    bool      mFirstTime = true;
    void     *mLibHandle = nullptr;
    bool    (*mAAudioStream_isMMap)(AAudioStream *stream) = nullptr;
    int32_t (*mAAudio_setMMapPolicy)(aaudio_policy_t policy) = nullptr;
    aaudio_policy_t (*mAAudio_getMMapPolicy)() = nullptr;
    bool      mMMapSupported = false;
    bool      mMMapExclusiveSupported = false;
};

#endif //OBOETESTER_AAUDIO_EXTENSIONS_H
