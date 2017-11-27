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
    close(); // TODO dangerous from a destructor, require caller to close()
}

AAudioLoader* AAudioLoader::getInstance() {
    static AAudioLoader instance;
    return &instance;
}

int AAudioLoader::open() {
    if (mLibHandle != nullptr) {
        return 0;
    }
    mLibHandle = dlopen(LIB_AAUDIO_NAME, 0);
    if (mLibHandle == nullptr) {
        LOGI("AAudioLoader::open() could not find " LIB_AAUDIO_NAME);
        return -1; // TODO review return code
    } else {
        LOGD("AAudioLoader():  dlopen(%s) returned %p", LIB_AAUDIO_NAME, mLibHandle);
    }

    // Load all the function pointers.
    createStreamBuilder = (aaudio_result_t (*)(AAudioStreamBuilder **builder))
            dlsym(mLibHandle, "AAudio_createStreamBuilder");

    builder_openStream = (aaudio_result_t (*)(AAudioStreamBuilder *builder,
                                              AAudioStream **stream))
            dlsym(mLibHandle, "AAudioStreamBuilder_openStream");

    builder_setChannelCount    = load_V_PBI("AAudioStreamBuilder_setChannelCount");
    if (builder_setChannelCount == nullptr) {
        // Use old alias if needed.
        builder_setChannelCount    = load_V_PBI("AAudioStreamBuilder_setSamplesPerFrame");
    }

    builder_setBufferCapacityInFrames = load_V_PBI("AAudioStreamBuilder_setBufferCapacityInFrames");
    builder_setDeviceId        = load_V_PBI("AAudioStreamBuilder_setDeviceId");
    builder_setDirection       = load_V_PBI("AAudioStreamBuilder_setDirection");
    builder_setFormat          = load_V_PBI("AAudioStreamBuilder_setFormat");
    builder_setFramesPerDataCallback = load_V_PBI("AAudioStreamBuilder_setFramesPerDataCallback");
    builder_setSharingMode     = load_V_PBI("AAudioStreamBuilder_setSharingMode");
    builder_setPerformanceMode     = load_V_PBI("AAudioStreamBuilder_setPerformanceMode");
    builder_setSampleRate      = load_V_PBI("AAudioStreamBuilder_setSampleRate");

    builder_delete             = load_I_PB("AAudioStreamBuilder_delete");

    stream_getFormat = (aaudio_format_t (*)(AAudioStream *stream))
            dlsym(mLibHandle, "AAudioStream_getFormat");

    builder_setDataCallback = (void (*)(AAudioStreamBuilder *builder,
                                        AAudioStream_dataCallback callback,
                                        void *userData))
            dlsym(mLibHandle, "AAudioStreamBuilder_setDataCallback");

    builder_setErrorCallback = (void (*)(AAudioStreamBuilder *builder,
                                        AAudioStream_errorCallback callback,
                                        void *userData))
            dlsym(mLibHandle, "AAudioStreamBuilder_setErrorCallback");

    stream_read = (aaudio_result_t (*)(AAudioStream *stream,
                                       void *buffer,
                                       int32_t numFrames,
                                       int64_t timeoutNanoseconds))
            dlsym(mLibHandle, "AAudioStream_read");

    stream_write = (aaudio_result_t (*)(AAudioStream *stream,
                                        const void *buffer,
                                        int32_t numFrames,
                                        int64_t timeoutNanoseconds))
            dlsym(mLibHandle, "AAudioStream_write");


    stream_waitForStateChange = (aaudio_result_t (*)(AAudioStream *stream,
                                                 aaudio_stream_state_t inputState,
                                                 aaudio_stream_state_t *nextState,
                                                 int64_t timeoutNanoseconds))
            dlsym(mLibHandle, "AAudioStream_waitForStateChange");


    stream_getTimestamp = (aaudio_result_t (*)(AAudioStream *stream,
                                           clockid_t clockid,
                                           int64_t *framePosition,
                                           int64_t *timeNanoseconds))
            dlsym(mLibHandle, "AAudioStream_getTimestamp");

    stream_getChannelCount    = load_I_PS("AAudioStream_getChannelCount");
    if (stream_getChannelCount == nullptr) {
        // Use old alias if needed.
        stream_getChannelCount    = load_I_PS("AAudioStream_getSamplesPerFrame");
    }

    stream_close              = load_I_PS("AAudioStream_close");

    stream_getBufferSize      = load_I_PS("AAudioStream_getBufferSizeInFrames");
    stream_getDeviceId        = load_I_PS("AAudioStream_getDeviceId");
    stream_getDirection       = load_I_PS("AAudioStream_getDirection");
    stream_getBufferCapacity  = load_I_PS("AAudioStream_getBufferCapacityInFrames");
    stream_getFramesPerBurst  = load_I_PS("AAudioStream_getFramesPerBurst");
    stream_getFramesRead      = load_L_PS("AAudioStream_getFramesRead");
    stream_getFramesWritten   = load_L_PS("AAudioStream_getFramesWritten");
    stream_getPerformanceMode = load_I_PS("AAudioStream_getPerformanceMode");
    stream_getSampleRate      = load_I_PS("AAudioStream_getSampleRate");
    stream_getSharingMode     = load_I_PS("AAudioStream_getSharingMode");
    stream_getState           = load_I_PS("AAudioStream_getState");
    stream_getXRunCount       = load_I_PS("AAudioStream_getXRunCount");

    stream_requestStart       = load_I_PS("AAudioStream_requestStart");
    stream_requestPause       = load_I_PS("AAudioStream_requestPause");
    stream_requestFlush       = load_I_PS("AAudioStream_requestFlush");
    stream_requestStop        = load_I_PS("AAudioStream_requestStop");

    stream_setBufferSize      = load_I_PSI("AAudioStream_setBufferSizeInFrames");

    convertResultToText       = load_PC_I("AAudio_convertResultToText");
    convertStreamStateToText  = load_PC_I("AAudio_convertStreamStateToText");

    return 0;
}

int AAudioLoader::close() {
    if (mLibHandle != nullptr) {
        dlclose(mLibHandle);
        mLibHandle = nullptr;
    }
    return 0;
}

static void AAudioLoader_check(void *proc, const char *functionName) {
    if (proc == nullptr) {
        LOGE("AAudioLoader could not find %s", functionName);
    } else {
        LOGV("AAudioLoader(): dlsym(%s) succeeded.", functionName);
    }
}

AAudioLoader::signature_PC_I AAudioLoader::load_PC_I(const char *functionName) {
    signature_PC_I proc = (signature_PC_I) dlsym(mLibHandle, functionName);
    AAudioLoader_check((void *)proc, functionName);
    return proc;
}

AAudioLoader::signature_V_PBI AAudioLoader::load_V_PBI(const char *functionName) {
    signature_V_PBI proc = (signature_V_PBI) dlsym(mLibHandle, functionName);
    AAudioLoader_check((void *)proc, functionName);
    return proc;
}

AAudioLoader::signature_I_PSI AAudioLoader::load_I_PSI(const char *functionName) {
    signature_I_PSI proc = (signature_I_PSI) dlsym(mLibHandle, functionName);
    AAudioLoader_check((void *)proc, functionName);
    return proc;
}

AAudioLoader::signature_I_PS AAudioLoader::load_I_PS(const char *functionName) {
    signature_I_PS proc = (signature_I_PS) dlsym(mLibHandle, functionName);
    AAudioLoader_check((void *)proc, functionName);
    return proc;
}

AAudioLoader::signature_L_PS AAudioLoader::load_L_PS(const char *functionName) {
    signature_L_PS proc = (signature_L_PS) dlsym(mLibHandle, functionName);
    AAudioLoader_check((void *)proc, functionName);
    return proc;
}

AAudioLoader::signature_I_PB AAudioLoader::load_I_PB(const char *functionName) {
    signature_I_PB proc = (signature_I_PB) dlsym(mLibHandle, functionName);
    AAudioLoader_check((void *)proc, functionName);
    return proc;
}

} // namespace oboe