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

#include <dlfcn.h>
#include <cstdio>
#include "Trace.h"
#include "OboeDebug.h"

using namespace oboe;

static char buffer[256];

// Tracing functions
static void *(*ATrace_beginSection)(const char *sectionName);

static void *(*ATrace_endSection)();

static void *(*ATrace_setCounter)(const char *counterName, int64_t counterValue);

static bool *(*ATrace_isEnabled)(void);

typedef void *(*fp_ATrace_beginSection)(const char *sectionName);

typedef void *(*fp_ATrace_endSection)();

typedef void *(*fp_ATrace_setCounter)(const char *counterName, int64_t counterValue);

typedef bool *(*fp_ATrace_isEnabled)(void);

bool Trace::mIsTracingEnabled = false;
bool Trace::mIsSetCounterSupported = false;
bool Trace::mHasErrorBeenShown = false;

void Trace::beginSection(const char *format, ...){
    if (mIsTracingEnabled) {
        va_list va;
        va_start(va, format);
        vsprintf(buffer, format, va);
        ATrace_beginSection(buffer);
        va_end(va);
    } else if (!mHasErrorBeenShown) {
        LOGE("Tracing is either not initialized (call Trace::initialize()) "
             "or not supported on this device");
        mHasErrorBeenShown = true;
    }
}

void Trace::endSection() {
    if (mIsTracingEnabled) {
        ATrace_endSection();
    }
}

void Trace::setCounter(const char *counterName, int64_t counterValue) {
    if (mIsSetCounterSupported) {
        ATrace_setCounter(counterName, counterValue);
    }
}

void Trace::initialize() {
    //LOGE("Trace::initialize");
    // Using dlsym allows us to use tracing on API 21+ without needing android/trace.h which wasn't
    // published until API 23
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) {
        LOGE("Could not open libandroid.so to dynamically load tracing symbols");
    } else {
        ATrace_beginSection =
                reinterpret_cast<fp_ATrace_beginSection >(
                        dlsym(lib, "ATrace_beginSection"));
        ATrace_endSection =
                reinterpret_cast<fp_ATrace_endSection >(
                        dlsym(lib, "ATrace_endSection"));
        ATrace_setCounter =
                reinterpret_cast<fp_ATrace_setCounter >(
                        dlsym(lib, "ATrace_setCounter"));
        ATrace_isEnabled =
                reinterpret_cast<fp_ATrace_isEnabled >(
                        dlsym(lib, "ATrace_isEnabled"));

        if (ATrace_beginSection != nullptr && ATrace_endSection != nullptr
                && ATrace_isEnabled != nullptr && ATrace_isEnabled()) {
            mIsTracingEnabled = true;
            if (ATrace_setCounter != nullptr) {
                mIsSetCounterSupported = true;
            } else {
                LOGE("setCounter not supported");
            }
        }
    }
}
