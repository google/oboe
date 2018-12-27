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

#ifndef OBOE_AAUDIO_LOADER_H_
#define OBOE_AAUDIO_LOADER_H_

#include <unistd.h>
#include "oboe/Definitions.h"

#include "aaudio/AAudio.h"

namespace oboe {

/**
 * The AAudio API was not available in early versions of Android.
 * To avoid linker errors, we dynamically link with the functions by name using dlsym().
 * On older versions this linkage will safely fail.
 */
class AAudioLoader {
  public:
    // Use signatures for common functions.
    typedef const char * (*signature_PC_I)(int32_t);
    typedef int32_t (*signature_I_I)(int32_t);
    typedef int32_t (*signature_I_II)(int32_t, int32_t);
    typedef int32_t (*signature_I_IPI)(int32_t, int32_t *);
    typedef int32_t (*signature_I_IIPI)(int32_t, int32_t, int32_t *);

    typedef int32_t (*signature_I_PB)(AAudioStreamBuilder *);  // AAudioStreamBuilder_delete()
    // AAudioStreamBuilder_setSampleRate()
    typedef void    (*signature_V_PBI)(AAudioStreamBuilder *, int32_t);

    typedef int32_t (*signature_I_PS)(AAudioStream *);  // AAudioStream_getSampleRate()
    typedef int64_t (*signature_L_PS)(AAudioStream *);  // AAudioStream_getFramesRead()
    // AAudioStream_setBufferSizeInFrames()
    typedef int32_t (*signature_I_PSI)(AAudioStream *, int32_t);

    static AAudioLoader* getInstance(); // singleton

    /**
     * Open the AAudio shared library and load the function pointers.
     * This can be called multiple times.
     * It should only be called from one thread.
     *
     * The destructor will clean up after the open.
     *
     * @return 0 if successful or negative error.
     */
    int open();

    // Function pointers into the AAudio shared library.
    aaudio_result_t (*createStreamBuilder)(AAudioStreamBuilder **builder) = nullptr;

    aaudio_result_t  (*builder_openStream)(AAudioStreamBuilder *builder,
                                           AAudioStream **stream) = nullptr;

    signature_V_PBI builder_setBufferCapacityInFrames = nullptr;
    signature_V_PBI builder_setChannelCount = nullptr;
    signature_V_PBI builder_setDeviceId = nullptr;
    signature_V_PBI builder_setDirection = nullptr;
    signature_V_PBI builder_setFormat = nullptr;
    signature_V_PBI builder_setFramesPerDataCallback = nullptr;
    signature_V_PBI builder_setPerformanceMode = nullptr;
    signature_V_PBI builder_setSampleRate = nullptr;
    signature_V_PBI builder_setSharingMode = nullptr;

    signature_V_PBI builder_setUsage = nullptr;
    signature_V_PBI builder_setContentType = nullptr;
    signature_V_PBI builder_setInputPreset = nullptr;
    signature_V_PBI builder_setSessionId = nullptr;

    void (*builder_setDataCallback)(AAudioStreamBuilder *builder,
                                    AAudioStream_dataCallback callback,
                                    void *userData) = nullptr;

    void (*builder_setErrorCallback)(AAudioStreamBuilder *builder,
                                    AAudioStream_errorCallback callback,
                                    void *userData) = nullptr;

    signature_I_PB  builder_delete = nullptr;

    aaudio_format_t (*stream_getFormat)(AAudioStream *stream) = nullptr;

    aaudio_result_t (*stream_read)(AAudioStream* stream,
                                   void *buffer,
                                   int32_t numFrames,
                                   int64_t timeoutNanoseconds) = nullptr;

    aaudio_result_t (*stream_write)(AAudioStream *stream,
                                   const void *buffer,
                                   int32_t numFrames,
                                   int64_t timeoutNanoseconds) = nullptr;

    aaudio_result_t (*stream_waitForStateChange)(AAudioStream *stream,
                                                 aaudio_stream_state_t inputState,
                                                 aaudio_stream_state_t *nextState,
                                                 int64_t timeoutNanoseconds) = nullptr;

    aaudio_result_t (*stream_getTimestamp)(AAudioStream *stream,
                                          clockid_t clockid,
                                          int64_t *framePosition,
                                          int64_t *timeNanoseconds) = nullptr;

    signature_I_PS   stream_close = nullptr;

    signature_I_PS   stream_getChannelCount = nullptr;
    signature_I_PS   stream_getDeviceId = nullptr;

    signature_I_PS   stream_getBufferSize = nullptr;
    signature_I_PS   stream_getBufferCapacity = nullptr;
    signature_I_PS   stream_getFramesPerBurst = nullptr;
    signature_I_PS   stream_getState = nullptr;
    signature_I_PS   stream_getPerformanceMode = nullptr;
    signature_I_PS   stream_getSampleRate = nullptr;
    signature_I_PS   stream_getSharingMode = nullptr;
    signature_I_PS   stream_getXRunCount = nullptr;

    signature_I_PSI  stream_setBufferSize = nullptr;
    signature_I_PS   stream_requestStart = nullptr;
    signature_I_PS   stream_requestPause = nullptr;
    signature_I_PS   stream_requestFlush = nullptr;
    signature_I_PS   stream_requestStop = nullptr;

    signature_L_PS   stream_getFramesRead = nullptr;
    signature_L_PS   stream_getFramesWritten = nullptr;

    signature_PC_I   convertResultToText = nullptr;

    signature_I_PS   stream_getUsage = nullptr;
    signature_I_PS   stream_getContentType = nullptr;
    signature_I_PS   stream_getInputPreset = nullptr;
    signature_I_PS   stream_getSessionId = nullptr;

  private:
    AAudioLoader() {}
    ~AAudioLoader();

    // Load function pointers for specific signatures.
    signature_PC_I   load_PC_I(const char *name);

    signature_V_PBI  load_V_PBI(const char *name);
    signature_I_PB   load_I_PB(const char *name);
    signature_I_PS   load_I_PS(const char *name);
    signature_L_PS   load_L_PS(const char *name);
    signature_I_PSI  load_I_PSI(const char *name);

    void *mLibHandle = nullptr;
};

} // namespace oboe

#endif //OBOE_AAUDIO_LOADER_H_
