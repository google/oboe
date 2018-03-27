/* Copyright 2015 The Android Open Source Project
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
#include <sys/types.h>
#include <cassert>
#include <android/log.h>


#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "common/OboeDebug.h"
#include "oboe/AudioStreamBuilder.h"
#include "AudioStreamOpenSLES.h"
#include "OpenSLESUtilities.h"

#ifndef NULL
#define NULL 0
#endif

#define DEFAULT_FRAMES_PER_CALLBACK   192    // 4 msec at 48000 Hz
#define DEFAULT_SAMPLE_RATE           48000  // very common rate for mobile audio and video
#define DEFAULT_CHANNEL_COUNT         2      // stereo

using namespace oboe;

AudioStreamOpenSLES::AudioStreamOpenSLES(const AudioStreamBuilder &builder)
    : AudioStreamBuffered(builder) {
    mSimpleBufferQueueInterface = NULL;
    mFramesPerBurst = builder.getDefaultFramesPerBurst();
    LOGD("AudioStreamOpenSLES(): after OpenSLContext()");
}

AudioStreamOpenSLES::~AudioStreamOpenSLES() {
    delete[] mCallbackBuffer;
}

static bool s_isLittleEndian() {
    static uint32_t value = 1;
    return *((uint8_t *) &value) == 1; // Does address point to LSB?
}

SLuint32 AudioStreamOpenSLES::getDefaultByteOrder() {
    return s_isLittleEndian() ? SL_BYTEORDER_LITTLEENDIAN : SL_BYTEORDER_BIGENDIAN;
}

Result AudioStreamOpenSLES::open() {

    LOGI("AudioStreamOpenSLES::open(chans:%d, rate:%d)",
                        mChannelCount, mSampleRate);

    if (getSdkVersion() < __ANDROID_API_L__ && mFormat == AudioFormat::Float){
        // TODO: Allow floating point format on API <21 using float->int16 converter
        return Result::ErrorInvalidFormat;
    }

    SLresult result = EngineOpenSLES::getInstance().open();
    if (SL_RESULT_SUCCESS != result) {
        return Result::ErrorInternal;
    }

    // If audio format is unspecified then choose a suitable default.
    // API 21+: FLOAT
    // API <21: INT16
    if (mFormat == AudioFormat::Unspecified){
        mFormat = (getSdkVersion() < __ANDROID_API_L__) ?
                  AudioFormat::I16 : AudioFormat::Float;
    }

    Result oboeResult = AudioStreamBuffered::open();
    if (oboeResult != Result::OK) {
        return oboeResult;
    }
    // Convert to defaults if UNSPECIFIED
    if (mSampleRate == kUnspecified) {
        mSampleRate = DEFAULT_SAMPLE_RATE;
    }
    if (mChannelCount == kUnspecified) {
        mChannelCount = DEFAULT_CHANNEL_COUNT;
    }

    // Decide frames per burst based hints from caller.
    // TODO  Can we query this from OpenSL ES?
    if (mFramesPerCallback != kUnspecified) {
        mFramesPerBurst = mFramesPerCallback;
    } else if (mFramesPerBurst != kUnspecified) { // set from defaultFramesPerBurst
        mFramesPerCallback = mFramesPerBurst;
    } else {
        mFramesPerBurst = mFramesPerCallback = DEFAULT_FRAMES_PER_CALLBACK;
    }

    mBytesPerCallback = mFramesPerCallback * getBytesPerFrame();
    delete[] mCallbackBuffer; // to prevent memory leaks
    mCallbackBuffer = new uint8_t[mBytesPerCallback];
    LOGD("AudioStreamOpenSLES(): mFramesPerCallback = %d", mFramesPerCallback);
    LOGD("AudioStreamOpenSLES(): mBytesPerCallback = %d", mBytesPerCallback);

    mSharingMode = SharingMode::Shared;

    if (!usingFIFO()) {
        mBufferCapacityInFrames = mFramesPerBurst * kBufferQueueLength;
    }

    return Result::OK;
}

Result AudioStreamOpenSLES::close() {
    if (mObjectInterface != nullptr) {
        (*mObjectInterface)->Destroy(mObjectInterface);
        mObjectInterface = nullptr;

    }
    mSimpleBufferQueueInterface = nullptr;
    EngineOpenSLES::getInstance().close();
    return Result::OK;
}

SLresult AudioStreamOpenSLES::enqueueCallbackBuffer(SLAndroidSimpleBufferQueueItf bq) {
    return (*bq)->Enqueue(bq, mCallbackBuffer, mBytesPerCallback);
}

SLresult AudioStreamOpenSLES::processBufferCallback(SLAndroidSimpleBufferQueueItf bq) {
    // Ask the callback to fill the output buffer with data.
    DataCallbackResult result = fireCallback(mCallbackBuffer, mFramesPerCallback);
    if (result != DataCallbackResult::Continue) {
        LOGE("Oboe callback returned %d", result);
        return SL_RESULT_INTERNAL_ERROR; // TODO How should we stop OpenSL ES.
    } else {
        // Pass the data to OpenSLES.
        return enqueueCallbackBuffer(bq);
    }
}

// this callback handler is called every time a buffer needs processing
static void bqCallbackGlue(SLAndroidSimpleBufferQueueItf bq, void *context) {
    ((AudioStreamOpenSLES *) context)->processBufferCallback(bq);
}

SLresult AudioStreamOpenSLES::registerBufferQueueCallback() {
    // The BufferQueue
    SLresult result = (*mObjectInterface)->GetInterface(mObjectInterface, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                &mSimpleBufferQueueInterface);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get buffer queue interface:%p result:%s",
             mSimpleBufferQueueInterface,
             getSLErrStr(result));
    } else {
        // Register the BufferQueue callback
        result = (*mSimpleBufferQueueInterface)->RegisterCallback(mSimpleBufferQueueInterface,
                                                                  bqCallbackGlue, this);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("RegisterCallback result:%s", getSLErrStr(result));
        }
    }
    return result;
}

int32_t AudioStreamOpenSLES::getFramesPerBurst() {
    return mFramesPerBurst;
}
