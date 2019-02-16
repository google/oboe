/*
 * Copyright 2016 The Android Open Source Project
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
#include "common/OboeDebug.h"
#include "AAudioLoader.h"

#define LIB_AAUDIO_NAME "libaaudio.so"

namespace oboe {

AAudioLoader::~AAudioLoader() {
    if (mLibHandle != nullptr) {
        dlclose(mLibHandle);
        mLibHandle = nullptr;
    }
}

AAudioLoader* AAudioLoader::getInstance() {
    static AAudioLoader instance;
    return &instance;
}

int AAudioLoader::open() {
    if (mLibHandle != nullptr) {
        return 0;
    }

    // Use RTLD_NOW to avoid the unpredictable behavior that RTLD_LAZY can cause.
    // Also resolving all the links now will prevent a run-time penalty later.
    mLibHandle = dlopen(LIB_AAUDIO_NAME, RTLD_NOW);
    if (mLibHandle == nullptr) {
        LOGI("AAudioLoader::open() could not find " LIB_AAUDIO_NAME);
        return -1; // TODO review return code
    } else {
        LOGD("AAudioLoader():  dlopen(%s) returned %p", LIB_AAUDIO_NAME, mLibHandle);
    }

    // Load all the function pointers.
    createStreamBuilder        = load_I_PPB("AAudio_createStreamBuilder");
    builder_openStream         = load_I_PBPPS("AAudioStreamBuilder_openStream");

    builder_setChannelCount    = load_V_PBI("AAudioStreamBuilder_setChannelCount");
    if (builder_setChannelCount == nullptr) {
        // Use old deprecated alias if needed.
        builder_setChannelCount = load_V_PBI("AAudioStreamBuilder_setSamplesPerFrame");
    }

    builder_setBufferCapacityInFrames = load_V_PBI("AAudioStreamBuilder_setBufferCapacityInFrames");
    builder_setDeviceId        = load_V_PBI("AAudioStreamBuilder_setDeviceId");
    builder_setDirection       = load_V_PBI("AAudioStreamBuilder_setDirection");
    builder_setFormat          = load_V_PBI("AAudioStreamBuilder_setFormat");
    builder_setFramesPerDataCallback = load_V_PBI("AAudioStreamBuilder_setFramesPerDataCallback");
    builder_setSharingMode     = load_V_PBI("AAudioStreamBuilder_setSharingMode");
    builder_setPerformanceMode = load_V_PBI("AAudioStreamBuilder_setPerformanceMode");
    builder_setSampleRate      = load_V_PBI("AAudioStreamBuilder_setSampleRate");

    builder_setUsage           = load_V_PBI("AAudioStreamBuilder_setUsage");
    builder_setContentType     = load_V_PBI("AAudioStreamBuilder_setContentType");
    builder_setInputPreset     = load_V_PBI("AAudioStreamBuilder_setInputPreset");
    builder_setSessionId       = load_V_PBI("AAudioStreamBuilder_setSessionId");

    builder_delete             = load_I_PB("AAudioStreamBuilder_delete");


    builder_setDataCallback    = load_V_PBPDPV("AAudioStreamBuilder_setDataCallback");
    builder_setErrorCallback   = load_V_PBPEPV("AAudioStreamBuilder_setErrorCallback");

    stream_read                = load_I_PSPVIL("AAudioStream_read");

    stream_write               = load_I_PSCPVIL("AAudioStream_write");

    stream_waitForStateChange  = load_I_PSTPTL("AAudioStream_waitForStateChange");

    stream_getTimestamp        = load_I_PSKPLPL("AAudioStream_getTimestamp");

    stream_isMMapUsed          = load_B_PS("AAudioStream_isMMapUsed");

    stream_getChannelCount     = load_I_PS("AAudioStream_getChannelCount");
    if (stream_getChannelCount == nullptr) {
        // Use old alias if needed.
        stream_getChannelCount    = load_I_PS("AAudioStream_getSamplesPerFrame");
    }

    stream_close               = load_I_PS("AAudioStream_close");

    stream_getBufferSize       = load_I_PS("AAudioStream_getBufferSizeInFrames");
    stream_getDeviceId         = load_I_PS("AAudioStream_getDeviceId");
    stream_getBufferCapacity   = load_I_PS("AAudioStream_getBufferCapacityInFrames");
    stream_getFormat           = load_F_PS("AAudioStream_getFormat");
    stream_getFramesPerBurst   = load_I_PS("AAudioStream_getFramesPerBurst");
    stream_getFramesRead       = load_L_PS("AAudioStream_getFramesRead");
    stream_getFramesWritten    = load_L_PS("AAudioStream_getFramesWritten");
    stream_getPerformanceMode  = load_I_PS("AAudioStream_getPerformanceMode");
    stream_getSampleRate       = load_I_PS("AAudioStream_getSampleRate");
    stream_getSharingMode      = load_I_PS("AAudioStream_getSharingMode");
    stream_getState            = load_I_PS("AAudioStream_getState");
    stream_getXRunCount        = load_I_PS("AAudioStream_getXRunCount");

    stream_requestStart        = load_I_PS("AAudioStream_requestStart");
    stream_requestPause        = load_I_PS("AAudioStream_requestPause");
    stream_requestFlush        = load_I_PS("AAudioStream_requestFlush");
    stream_requestStop         = load_I_PS("AAudioStream_requestStop");

    stream_setBufferSize       = load_I_PSI("AAudioStream_setBufferSizeInFrames");

    convertResultToText        = load_CPH_I("AAudio_convertResultToText");

    stream_getUsage            = load_I_PS("AAudioStream_getUsage");
    stream_getContentType      = load_I_PS("AAudioStream_getContentType");
    stream_getInputPreset      = load_I_PS("AAudioStream_getInputPreset");
    stream_getSessionId        = load_I_PS("AAudioStream_getSessionId");

    return 0;
}

static void AAudioLoader_check(void *proc, const char *functionName) {
    if (proc == nullptr) {
        LOGW("AAudioLoader could not find %s", functionName);
    }
}

AAudioLoader::signature_I_PPB AAudioLoader::load_I_PPB(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PPB>(proc);
}

AAudioLoader::signature_CPH_I AAudioLoader::load_CPH_I(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_CPH_I>(proc);
}

AAudioLoader::signature_V_PBI AAudioLoader::load_V_PBI(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_V_PBI>(proc);
}

AAudioLoader::signature_V_PBPDPV AAudioLoader::load_V_PBPDPV(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_V_PBPDPV>(proc);
}

AAudioLoader::signature_V_PBPEPV AAudioLoader::load_V_PBPEPV(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_V_PBPEPV>(proc);
}

AAudioLoader::signature_I_PSI AAudioLoader::load_I_PSI(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PSI>(proc);
}

AAudioLoader::signature_I_PS AAudioLoader::load_I_PS(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PS>(proc);
}

AAudioLoader::signature_L_PS AAudioLoader::load_L_PS(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_L_PS>(proc);
}

AAudioLoader::signature_F_PS AAudioLoader::load_F_PS(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_F_PS>(proc);
}

AAudioLoader::signature_B_PS AAudioLoader::load_B_PS(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_B_PS>(proc);
}

AAudioLoader::signature_I_PB AAudioLoader::load_I_PB(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PB>(proc);
}

AAudioLoader::signature_I_PBPPS AAudioLoader::load_I_PBPPS(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PBPPS>(proc);
}

AAudioLoader::signature_I_PSCPVIL AAudioLoader::load_I_PSCPVIL(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PSCPVIL>(proc);
}

AAudioLoader::signature_I_PSPVIL AAudioLoader::load_I_PSPVIL(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PSPVIL>(proc);
}

AAudioLoader::signature_I_PSTPTL AAudioLoader::load_I_PSTPTL(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PSTPTL>(proc);
}

AAudioLoader::signature_I_PSKPLPL AAudioLoader::load_I_PSKPLPL(const char *functionName) {
    void *proc = dlsym(mLibHandle, functionName);
    AAudioLoader_check(proc, functionName);
    return reinterpret_cast<signature_I_PSKPLPL>(proc);
}

} // namespace oboe
