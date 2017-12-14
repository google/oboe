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


OpenSLEngine *OpenSLEngine::getInstance() {
    // TODO mutex
    if (sInstance == nullptr) {
        sInstance = new OpenSLEngine();
    }
    return sInstance;
}

SLresult OpenSLEngine::open() {
    SLresult result = SL_RESULT_SUCCESS;
    if (sOpenCount > 0) {
        ++sOpenCount;
        return SL_RESULT_SUCCESS;
    }

    // create engine
    result = slCreateEngine(&sEngineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLEngine() - slCreateEngine() result:%s", getSLErrStr(result));
        return result;
    }

    // realize the engine
    result = (*sEngineObject)->Realize(sEngineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLEngine() - Realize() engine result:%s", getSLErrStr(result));
        goto error;
    }

    // get the engine interface, which is needed in order to create other objects
    result = (*sEngineObject)->GetInterface(sEngineObject, SL_IID_ENGINE, &sEngineEngine);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLEngine() - GetInterface() engine result:%s", getSLErrStr(result));
        goto error;
    }

    ++sOpenCount;
    return result;

    error:
    close();
    return result;
}

void OpenSLEngine::close() {
//    __android_log_print(ANDROID_LOG_INFO, TAG, "CloseSLEngine()");
    --sOpenCount;
    if (sOpenCount > 0) {
        return;
    }

    if (sEngineObject != NULL) {
        (*sEngineObject)->Destroy(sEngineObject);
        sEngineObject = NULL;
        sEngineEngine = NULL;
    }
}

SLresult OpenSLEngine::createOutputMix(SLObjectItf *objectItf) {
    return (*sEngineEngine)->CreateOutputMix(sEngineEngine, objectItf, 0, 0, 0);
}

SLresult OpenSLEngine::createAudioPlayer(SLObjectItf *objectItf,
                           SLDataSource *audioSource,
                           SLDataSink *audioSink) {

    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean reqs[] = {SL_BOOLEAN_TRUE};

    // The Player
    return (*sEngineEngine)->CreateAudioPlayer(sEngineEngine, objectItf, audioSource,
                                                 audioSink,
                                                 sizeof(ids) / sizeof(ids[0]), ids, reqs);
}

SLresult OpenSLEngine::createAudioRecorder(SLObjectItf *objectItf,
                                         SLDataSource *audioSource,
                                         SLDataSink *audioSink) {

    const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                 SL_IID_ANDROIDCONFIGURATION };
    const SLboolean reqs[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    // The Player
    return (*sEngineEngine)->CreateAudioRecorder(sEngineEngine, objectItf, audioSource,
                                               audioSink,
                                               sizeof(ids) / sizeof(ids[0]), ids, reqs);
}

OpenSLEngine *OpenSLEngine::sInstance = nullptr;

OpenSLOutputMixer *OpenSLOutputMixer::getInstance() {
    // TODO mutex
    if (sInstance == nullptr) {
        sInstance = new OpenSLOutputMixer();
    }
    return sInstance;
}

SLresult OpenSLOutputMixer::open() {
    SLresult result = SL_RESULT_SUCCESS;
    if (sOpenCount > 0) {
        ++sOpenCount;
        return SL_RESULT_SUCCESS;
    }

    // get the output mixer
    result = OpenSLEngine::getInstance()->createOutputMix(&sOutputMixObject);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLOutputMixer() - createOutputMix() result:%s", getSLErrStr(result));
        goto error;
    }

    // realize the output mix
    result = (*sOutputMixObject)->Realize(sOutputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLOutputMixer() - Realize() sOutputMixObject result:%s", getSLErrStr(result));
        goto error;
    }

    ++sOpenCount;
    return result;

    error:
    close();
    return result;
}

void OpenSLOutputMixer::close() {
//    __android_log_print(ANDROID_LOG_INFO, TAG, "CloseSLEngine()");
    --sOpenCount;
    if (sOpenCount > 0) {
        return;
    }
    // destroy output mix object, and invalidate all associated interfaces
    if (sOutputMixObject != NULL) {
        (*sOutputMixObject)->Destroy(sOutputMixObject);
        sOutputMixObject = NULL;
    }
}

SLresult OpenSLOutputMixer::createAudioPlayer(SLObjectItf *objectItf,
                           SLDataSource *audioSource) {
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, sOutputMixObject};
    SLDataSink audioSink = {&loc_outmix, NULL};
    return OpenSLEngine::getInstance()->createAudioPlayer(objectItf, audioSource, &audioSink);
}

OpenSLOutputMixer *OpenSLOutputMixer::sInstance = nullptr;


AudioStreamOpenSLES::AudioStreamOpenSLES(const AudioStreamBuilder &builder)
    : AudioStreamBuffered(builder) {
    mSimpleBufferQueueInterface = NULL;
    mFramesPerBurst = builder.getDefaultFramesPerBurst();
    OpenSLEngine::getInstance()->open();
    LOGD("AudioStreamOpenSLES(): after OpenSLContext()");
}

AudioStreamOpenSLES::~AudioStreamOpenSLES() {
    OpenSLEngine::getInstance()->close();
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

    if (__ANDROID_API__ < __ANDROID_API_L__ && mFormat == AudioFormat::Float){
        // TODO: Allow floating point format on API <21 using float->int16 converter
        return Result::ErrorInvalidFormat;
    }

    // If audio format is unspecified then choose a suitable default.
    // API 21+: FLOAT
    // API <21: INT16
    if (mFormat == AudioFormat::Unspecified){
        mFormat = (__ANDROID_API__ < __ANDROID_API_L__) ?
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
    mCallbackBuffer = new uint8_t[mBytesPerCallback];
    LOGD("AudioStreamOpenSLES(): mFramesPerCallback = %d", mFramesPerCallback);
    LOGD("AudioStreamOpenSLES(): mBytesPerCallback = %d", mBytesPerCallback);

    mSharingMode = SharingMode::Shared;
    mBufferCapacityInFrames = mFramesPerBurst * mBurstsPerBuffer;

    return Result::OK;
}

Result AudioStreamOpenSLES::close() {
    if (mObjectInterface != NULL) {
        (*mObjectInterface)->Destroy(mObjectInterface);
        mObjectInterface = NULL;

    }
    mSimpleBufferQueueInterface = NULL;
    return Result::OK;
}

SLresult AudioStreamOpenSLES::enqueueCallbackBuffer(SLAndroidSimpleBufferQueueItf bq) {
    return (*bq)->Enqueue(bq, mCallbackBuffer, mBytesPerCallback);
}

SLresult AudioStreamOpenSLES::processBufferCallback(SLAndroidSimpleBufferQueueItf bq) {
    // Ask the callback to fill the output buffer with data.
    Result result = fireCallback(mCallbackBuffer, mFramesPerCallback);
    if (result != Result::OK) {
        LOGE("Oboe callback returned %d", result);
        return SL_RESULT_INTERNAL_ERROR;
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
        LOGE("get bufferqueue interface:%p result:%s", mSimpleBufferQueueInterface, getSLErrStr(result));
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
