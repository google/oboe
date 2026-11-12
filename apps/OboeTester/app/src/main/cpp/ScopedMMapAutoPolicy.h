/*
 * Copyright 2026 The Android Open Source Project
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

#ifndef OBOETESTER_SCOPED_MMAP_AUTO_POLICY_H
#define OBOETESTER_SCOPED_MMAP_AUTO_POLICY_H

#include <aaudio/AAudioExtensions.h>
#include <aaudio/AAudioLoader.h>

#include "oboe/Oboe.h"

/**
 * RAII helper that temporarily enables AAudio MMAP policy (AUTO) when the current per-process
 * policy is disabled (e.g., set to NEVER by another OboeTester screen), restoring the exact
 * prior policy on scope exit without clobbering UNSPECIFIED into NEVER.
 */
class ScopedMMapAutoPolicy {
public:
    ScopedMMapAutoPolicy() {
        mLoader = oboe::AAudioLoader::getInstance();
        if (mLoader != nullptr && mLoader->open() == 0 &&
            mLoader->aaudio_getMMapPolicy != nullptr &&
            mLoader->aaudio_setMMapPolicy != nullptr) {
            mSavedPolicy = mLoader->aaudio_getMMapPolicy();
            if (static_cast<oboe::MMapPolicy>(mSavedPolicy) == oboe::MMapPolicy::Never ||
                !oboe::AAudioExtensions::getInstance().isMMapEnabled()) {
                mLoader->aaudio_setMMapPolicy(
                        static_cast<aaudio_policy_t>(oboe::MMapPolicy::Auto));
                mChangedViaLoader = true;
            }
        } else if (oboe::AAudioExtensions::getInstance().isMMapSupported() &&
                   !oboe::AAudioExtensions::getInstance().isMMapEnabled()) {
            oboe::AAudioExtensions::getInstance().setMMapEnabled(true);
            mChangedViaExt = true;
        }
    }

    ~ScopedMMapAutoPolicy() {
        if (mChangedViaLoader && mLoader != nullptr && mLoader->aaudio_setMMapPolicy != nullptr) {
            mLoader->aaudio_setMMapPolicy(mSavedPolicy);
        } else if (mChangedViaExt) {
            oboe::AAudioExtensions::getInstance().setMMapEnabled(false);
        }
    }

    ScopedMMapAutoPolicy(const ScopedMMapAutoPolicy &) = delete;
    ScopedMMapAutoPolicy &operator=(const ScopedMMapAutoPolicy &) = delete;

private:
    oboe::AAudioLoader *mLoader = nullptr;
    aaudio_policy_t mSavedPolicy = 0;
    bool mChangedViaLoader = false;
    bool mChangedViaExt = false;
};

#endif // OBOETESTER_SCOPED_MMAP_AUTO_POLICY_H
