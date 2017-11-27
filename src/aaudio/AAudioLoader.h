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
     * @return 0 if successful or negative error.
     */
    int open();

    /**
     * Close the AAudio shared library.
     * This can be called multiple times.
     * It should only be called from one thread.
     *
     * The open() and close() do not nest. Calling close() once will always close the library.
     * The destructor will call close() so you don't need to.
     *
     * @return 0 if successful or negative error.
     */
    int close();

    // Function pointers into the AAudio shared library.
    aaudio_result_t (*createStreamBuilder)(AAudioStreamBuilder **builder);

    aaudio_result_t  (*builder_openStream)(AAudioStreamBuilder *builder,
                                           AAudioStream **stream);

    signature_V_PBI builder_setBufferCapacityInFrames;
    signature_V_PBI builder_setChannelCount;
    signature_V_PBI builder_setDeviceId;
    signature_V_PBI builder_setDirection;
    signature_V_PBI builder_setFormat;
    signature_V_PBI builder_setFramesPerDataCallback;
    signature_V_PBI builder_setPerformanceMode;
    signature_V_PBI builder_setSampleRate;
    signature_V_PBI builder_setSharingMode;

    void (*builder_setDataCallback)(AAudioStreamBuilder *builder,
                                    AAudioStream_dataCallback callback,
                                    void *userData);

    void (*builder_setErrorCallback)(AAudioStreamBuilder *builder,
                                    AAudioStream_errorCallback callback,
                                    void *userData);

    signature_I_PB  builder_delete;

    aaudio_format_t (*stream_getFormat)(AAudioStream *stream);

    aaudio_result_t (*stream_read)(AAudioStream* stream,
                                   void *buffer,
                                   int32_t numFrames,
                                   int64_t timeoutNanoseconds);

    aaudio_result_t (*stream_write)(AAudioStream *stream,
                                   const void *buffer,
                                   int32_t numFrames,
                                   int64_t timeoutNanoseconds);

    aaudio_result_t (*stream_waitForStateChange)(AAudioStream *stream,
                                                 aaudio_stream_state_t inputState,
                                                 aaudio_stream_state_t *nextState,
                                                 int64_t timeoutNanoseconds);

    aaudio_result_t (*stream_getTimestamp)(AAudioStream *stream,
                                          clockid_t clockid,
                                          int64_t *framePosition,
                                          int64_t *timeNanoseconds);

    signature_I_PS   stream_close;

    signature_I_PS   stream_getChannelCount;
    signature_I_PS   stream_getDeviceId;
    signature_I_PS   stream_getDirection;
    signature_I_PS   stream_getBufferSize;
    signature_I_PS   stream_getBufferCapacity;
    signature_I_PS   stream_getFramesPerBurst;
    signature_I_PS   stream_getState;
    signature_I_PS   stream_getPerformanceMode;
    signature_I_PS   stream_getSampleRate;
    signature_I_PS   stream_getSharingMode;
    signature_I_PS   stream_getXRunCount;

    signature_I_PSI  stream_setBufferSize;
    signature_I_PS   stream_requestStart;
    signature_I_PS   stream_requestPause;
    signature_I_PS   stream_requestFlush;
    signature_I_PS   stream_requestStop;

    signature_L_PS   stream_getFramesRead;
    signature_L_PS   stream_getFramesWritten;

    signature_PC_I   convertResultToText;
    signature_PC_I   convertStreamStateToText;

    // TODO add any missing AAudio functions.

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
