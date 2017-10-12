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
#include "oboe/OboeStreamBuilder.h"
#include "OboeStreamOpenSLES.h"
#include "OpenSLESUtilities.h"

#ifndef NULL
#define NULL 0
#endif

#define DEFAULT_FRAMES_PER_CALLBACK   192    // 4 msec at 48000 Hz
#define DEFAULT_SAMPLE_RATE           48000  // very common rate for mobile audio and video
#define DEFAULT_CHANNEL_COUNT         2      // stereo

#define OBOE_BITS_PER_BYTE            8      // common value

/*
 * OSLES Helpers
 */
static const char *errStrings[] = {
        "SL_RESULT_SUCCESS",                  // 0
        "SL_RESULT_PRECONDITIONS_VIOLATE",    // 1
        "SL_RESULT_PARAMETER_INVALID",        // 2
        "SL_RESULT_MEMORY_FAILURE",           // 3
        "SL_RESULT_RESOURCE_ERROR",           // 4
        "SL_RESULT_RESOURCE_LOST",            // 5
        "SL_RESULT_IO_ERROR",                 // 6
        "SL_RESULT_BUFFER_INSUFFICIENT",      // 7
        "SL_RESULT_CONTENT_CORRUPTED",        // 8
        "SL_RESULT_CONTENT_UNSUPPORTED",      // 9
        "SL_RESULT_CONTENT_NOT_FOUND",        // 10
        "SL_RESULT_PERMISSION_DENIED",        // 11
        "SL_RESULT_FEATURE_UNSUPPORTED",      // 12
        "SL_RESULT_INTERNAL_ERROR",           // 13
        "SL_RESULT_UNKNOWN_ERROR",            // 14
        "SL_RESULT_OPERATION_ABORTED",        // 15
        "SL_RESULT_CONTROL_LOST"              // 16
};

const char *getSLErrStr(int code) {
    return errStrings[code];
}

// These will wind up in <SLES/OpenSLES_Android.h>
#define SL_ANDROID_SPEAKER_QUAD (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT \
 | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT)

#define SL_ANDROID_SPEAKER_5DOT1 (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT \
 | SL_SPEAKER_FRONT_CENTER  | SL_SPEAKER_LOW_FREQUENCY| SL_SPEAKER_BACK_LEFT \
 | SL_SPEAKER_BACK_RIGHT)

#define SL_ANDROID_SPEAKER_7DOT1 (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT \
 |SL_SPEAKER_SIDE_RIGHT)

int chanCountToChanMask(int chanCount) {
    int channelMask = 0;

    switch (chanCount) {
        case  1:
            channelMask = SL_SPEAKER_FRONT_CENTER;
            break;

        case  2:
            channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
            break;

        case  4: // Quad
            channelMask = SL_ANDROID_SPEAKER_QUAD;
            break;

        case  6: // 5.1
            channelMask = SL_ANDROID_SPEAKER_5DOT1;
            break;

        case  8: // 7.1
            channelMask = SL_ANDROID_SPEAKER_7DOT1;
            break;
    }
    return channelMask;
}

static const char *TAG = "AAudioStreamOpenSLES";

// engine interfaces
static int32_t sOpenCount = 0;
static SLObjectItf sEngineObject = 0;
static SLEngineItf sEngineEngine;

// output mix interfaces
static SLObjectItf sOutputMixObject = 0;

static void CloseSLEngine();

SLresult OboeStreamOpenSLES::enqueueBuffer() {
    // Ask the callback to fill the output buffer with data.
    oboe_result_t result = fireCallback(mCallbackBuffer, mFramesPerCallback);
    if (result != OBOE_OK) {
        LOGE("Oboe callback returned %d", result);
        return SL_RESULT_INTERNAL_ERROR;
    } else {
        // Pass the data to OpenSLES.
        return (*bq_)->Enqueue(bq_, mCallbackBuffer, mBytesPerCallback);
    }
}

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    ((OboeStreamOpenSLES *) context)->enqueueBuffer();
}

static SLresult OpenSLEngine() {
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

    // get the output mixer
    result = (*sEngineEngine)->CreateOutputMix(sEngineEngine, &sOutputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLEngine() - CreateOutputMix() result:%s", getSLErrStr(result));
        goto error;
    }

    // realize the output mix
    result = (*sOutputMixObject)->Realize(sOutputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("OpenSLEngine() - Realize() sOutputMixObject result:%s", getSLErrStr(result));
        goto error;
    }

    ++sOpenCount;
    return result;

error:
    CloseSLEngine();
    return result;
}

static void CloseSLEngine() {
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

    if (sEngineObject != NULL) {
        (*sEngineObject)->Destroy(sEngineObject);
        sEngineObject = NULL;
        sEngineEngine = NULL;
    }
}

OboeStreamOpenSLES::OboeStreamOpenSLES(const OboeStreamBuilder &builder)
    : OboeStreamBuffered(builder) {
    bqPlayerObject_ = NULL;
    bq_ = NULL;
    bqPlayerPlay_ = NULL;
    mFramesPerBurst = builder.getDefaultFramesPerBurst();
    OpenSLEngine();
    LOGD("OboeStreamOpenSLES(): after OpenSLEngine()");
}

OboeStreamOpenSLES::~OboeStreamOpenSLES() {
    CloseSLEngine();
    delete[] mCallbackBuffer;
}

static SLuint32 ConvertFormatToRepresentation(oboe_audio_format_t format) {
    switch(format) {
        case OBOE_AUDIO_FORMAT_INVALID:
        case OBOE_AUDIO_FORMAT_UNSPECIFIED:
            return 0;
        case OBOE_AUDIO_FORMAT_PCM_I16:
            return SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;
        case OBOE_AUDIO_FORMAT_PCM_FLOAT:
            return SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    }
    return 0;
}

static bool s_isLittleEndian() {
    static uint32_t value = 1;
    return *((uint8_t *) &value) == 1; // Does address point to LSB?
}

static SLuint32 s_getDefaultByteOrder() {
    return s_isLittleEndian() ? SL_BYTEORDER_LITTLEENDIAN : SL_BYTEORDER_BIGENDIAN;
}

oboe_result_t OboeStreamOpenSLES::open() {

    SLresult result;

    __android_log_print(ANDROID_LOG_INFO, TAG, "AudioPlayerOpenSLES::Open(chans:%d, rate:%d)",
                        mChannelCount, mSampleRate);

    if (__ANDROID_API__ < __ANDROID_API_L__ && mFormat == OBOE_AUDIO_FORMAT_PCM_FLOAT){
        // TODO: Allow floating point format on API <21 using float->int16 converter
        return OBOE_ERROR_INVALID_FORMAT;
    }

    // If audio format is unspecified then choose a suitable default.
    // API 21+: FLOAT
    // API <21: INT16
    if (mFormat == OBOE_AUDIO_FORMAT_UNSPECIFIED){
        mFormat = (__ANDROID_API__ < __ANDROID_API_L__) ?
                  OBOE_AUDIO_FORMAT_PCM_I16 : OBOE_AUDIO_FORMAT_PCM_FLOAT;
    }

    oboe_result_t oboeResult = OboeStreamBuffered::open();
    if (oboeResult < 0) {
        return oboeResult;
    }
    // Convert to defaults if UNSPECIFIED
    if (mSampleRate == OBOE_UNSPECIFIED) {
        mSampleRate = DEFAULT_SAMPLE_RATE;
    }
    if (mChannelCount == OBOE_UNSPECIFIED) {
        mChannelCount = DEFAULT_CHANNEL_COUNT;
    }

    // Decide frames per burst based hints from caller.
    // TODO  Can we query this from OpenSL ES?
    if (mFramesPerCallback != OBOE_UNSPECIFIED) {
        mFramesPerBurst = mFramesPerCallback;
    } else if (mFramesPerBurst != OBOE_UNSPECIFIED) { // set from defaultFramesPerBurst
        mFramesPerCallback = mFramesPerBurst;
    } else {
        mFramesPerBurst = mFramesPerCallback = DEFAULT_FRAMES_PER_CALLBACK;
    }

    mBytesPerCallback = mFramesPerCallback * getBytesPerFrame();
    mCallbackBuffer = new uint8_t[mBytesPerCallback];
    LOGD("OboeStreamOpenSLES(): mFramesPerCallback = %d", mFramesPerCallback);
    LOGD("OboeStreamOpenSLES(): mBytesPerCallback = %d", mBytesPerCallback);

    SLuint32 bitsPerSample = getBytesPerSample() * OBOE_BITS_PER_BYTE;

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,    // locatorType
            static_cast<SLuint32>(mBurstsPerBuffer)};   // numBuffers

    // SLuint32 chanMask = SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT;

    // Define the audio data format.
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,       // formatType
            (SLuint32) mChannelCount,           // numChannels
            (SLuint32) (mSampleRate * OBOE_MILLIS_PER_SECOND),    // milliSamplesPerSec
            bitsPerSample,                      // bitsPerSample
            bitsPerSample,                      // containerSize;
            (SLuint32) chanCountToChanMask(mChannelCount), // channelMask
            s_getDefaultByteOrder(),
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    /**
     * API 21 (Lollipop) introduced support for floating-point data representation and an extended
     * data format type: SLAndroidDataFormat_PCM_EX. If running on API 21+ use this newer format
     * type, creating it from our original format.
     */
    if (__ANDROID_API__ >= __ANDROID_API_L__) {
        SLuint32 representation = ConvertFormatToRepresentation(getFormat());
        SLAndroidDataFormat_PCM_EX format_pcm_ex = OpenSLES_createExtendedFormat(format_pcm,
                                                                                 representation);
        // Overwrite the previous format.
        audioSrc.pFormat = &format_pcm_ex;
    }

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, sOutputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};

    // The Player
    result = (*sEngineEngine)->CreateAudioPlayer(sEngineEngine, &bqPlayerObject_, &audioSrc,
                                                 &audioSnk,
                                                sizeof(ids) / sizeof(ids[0]), ids, req);
    LOGD("CreateAudioPlayer() result:%s", getSLErrStr(result));
    assert(SL_RESULT_SUCCESS == result);

    result = (*bqPlayerObject_)->Realize(bqPlayerObject_, SL_BOOLEAN_FALSE);
    LOGD("Realize player object result:%s", getSLErrStr(result));
    assert(SL_RESULT_SUCCESS == result);

    result = (*bqPlayerObject_)->GetInterface(bqPlayerObject_, SL_IID_PLAY, &bqPlayerPlay_);
    LOGD("get player interface result:%s", getSLErrStr(result));
    assert(SL_RESULT_SUCCESS == result);

    // The BufferQueue
    result = (*bqPlayerObject_)->GetInterface(bqPlayerObject_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                              &bq_);
    LOGD("get bufferqueue interface:%p result:%s", bq_, getSLErrStr(result));
    assert(SL_RESULT_SUCCESS == result);

    // The register BufferQueue callback
    result = (*bq_)->RegisterCallback(bq_, bqPlayerCallback, this);
    LOGD("register callback result:%s", getSLErrStr(result));
    assert(SL_RESULT_SUCCESS == result);

    mSharingMode = OBOE_SHARING_MODE_SHARED;
    mBufferCapacityInFrames = mFramesPerBurst * mBurstsPerBuffer;

    return OBOE_OK;
}

oboe_result_t OboeStreamOpenSLES::close() {
//    __android_log_write(ANDROID_LOG_INFO, TAG, "OboeStreamOpenSLES()");
    // TODO make sure callback is no longer being called
    if (bqPlayerObject_ != NULL) {
        (*bqPlayerObject_)->Destroy(bqPlayerObject_);
        bqPlayerObject_ = NULL;

        // invalidate any interfaces
        bqPlayerPlay_ = NULL;
        bq_ = NULL;
    }
    return OBOE_OK;
}

oboe_result_t OboeStreamOpenSLES::setPlayState(SLuint32 newState)
{
    oboe_result_t result = OBOE_OK;
    LOGD("OboeStreamOpenSLES(): setPlayState()");
    if (bqPlayerPlay_ == NULL) {
        return OBOE_ERROR_INVALID_STATE;
    }
    SLresult slResult = (*bqPlayerPlay_)->SetPlayState(bqPlayerPlay_, newState);
    if(SL_RESULT_SUCCESS != slResult) {
        LOGD("OboeStreamOpenSLES(): setPlayState() returned %s", getSLErrStr(result));
        result = OBOE_ERROR_INVALID_STATE; // TODO review
    } else {
        setState(OBOE_STREAM_STATE_PAUSING);
    }
    return result;
}

oboe_result_t OboeStreamOpenSLES::requestStart()
{
    LOGD("OboeStreamOpenSLES(): requestStart()");
    oboe_result_t result = setPlayState(SL_PLAYSTATE_PLAYING);
    if(result != OBOE_OK) {
        result = OBOE_ERROR_INVALID_STATE; // TODO review
    } else {
        enqueueBuffer();
        setState(OBOE_STREAM_STATE_STARTING);
    }
    return result;
}


oboe_result_t OboeStreamOpenSLES::requestPause() {
    LOGD("OboeStreamOpenSLES(): requestPause()");
    oboe_result_t result = setPlayState(SL_PLAYSTATE_PAUSED);
    if(result != OBOE_OK) {
        result = OBOE_ERROR_INVALID_STATE; // TODO review
    } else {
        setState(OBOE_STREAM_STATE_PAUSING);
    }
    return result;
}

oboe_result_t OboeStreamOpenSLES::requestFlush() {
    LOGD("OboeStreamOpenSLES(): requestFlush()");
    if (bqPlayerPlay_ == NULL) {
        return OBOE_ERROR_INVALID_STATE;
    }
    return OBOE_ERROR_UNIMPLEMENTED; // TODO
}

oboe_result_t OboeStreamOpenSLES::requestStop()
{
    LOGD("OboeStreamOpenSLES(): requestStop()");
    oboe_result_t result = setPlayState(SL_PLAYSTATE_STOPPED);
    if(result != OBOE_OK) {
        result = OBOE_ERROR_INVALID_STATE; // TODO review
    } else {
        setState(OBOE_STREAM_STATE_STOPPING);
    }
    return result;
}

oboe_result_t OboeStreamOpenSLES::waitForStateChange(oboe_stream_state_t currentState,
                                                      oboe_stream_state_t *nextState,
                                                      int64_t timeoutNanoseconds)
{
    LOGD("OboeStreamOpenSLES(): waitForStateChange()");
    if (bqPlayerPlay_ == NULL) {
        return OBOE_ERROR_INVALID_STATE;
    }
    return OBOE_ERROR_UNIMPLEMENTED; // TODO
}

int32_t OboeStreamOpenSLES::getFramesPerBurst() {
    return mFramesPerBurst;
}

