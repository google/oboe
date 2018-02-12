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

#define OBOE_BITS_PER_BYTE            8      // common value

using namespace oboe;

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

const char *getSLErrStr(SLresult code) {
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
    const SLboolean req[] = {SL_BOOLEAN_TRUE};

    // The Player
    return (*sEngineEngine)->CreateAudioPlayer(sEngineEngine, objectItf, audioSource,
                                                 audioSink,
                                                 sizeof(ids) / sizeof(ids[0]), ids, req);
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

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    ((AudioStreamOpenSLES *) context)->enqueueBuffer();
}

SLresult AudioStreamOpenSLES::enqueueBuffer() {
    // Ask the callback to fill the output buffer with data.
    Result result = fireCallback(mCallbackBuffer, mFramesPerCallback);
    if (result != Result::OK) {
        LOGE("Oboe callback returned %d", result);
        return SL_RESULT_INTERNAL_ERROR;
    } else {
        // Pass the data to OpenSLES.
        return (*bq_)->Enqueue(bq_, mCallbackBuffer, mBytesPerCallback);
    }
}


static void CloseSLEngine() {
    OpenSLOutputMixer::getInstance()->close();
    OpenSLEngine::getInstance()->close();
}

static SLresult OpenSLContext() {
    SLresult result = OpenSLEngine::getInstance()->open();
    if (SL_RESULT_SUCCESS == result) {
        // get the output mixer
        result = OpenSLOutputMixer::getInstance()->open();
        if (SL_RESULT_SUCCESS != result) {
            CloseSLEngine();
        }
    }
    return result;
}

AudioStreamOpenSLES::AudioStreamOpenSLES(const AudioStreamBuilder &builder)
    : AudioStreamBuffered(builder) {
    bqPlayerObject_ = NULL;
    bq_ = NULL;
    bqPlayerPlay_ = NULL;
    mFramesPerBurst = builder.getDefaultFramesPerBurst();
    OpenSLContext();
    LOGD("AudioStreamOpenSLES(): after OpenSLContext()");
}

AudioStreamOpenSLES::~AudioStreamOpenSLES() {
    CloseSLEngine();
    delete[] mCallbackBuffer;
}

static SLuint32 ConvertFormatToRepresentation(AudioFormat format) {
    switch(format) {
        case AudioFormat::Invalid:
        case AudioFormat::Unspecified:
            return 0;
        case AudioFormat::I16:
            return SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;
        case AudioFormat::Float:
            return SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    }
}

static bool s_isLittleEndian() {
    static uint32_t value = 1;
    return *((uint8_t *) &value) == 1; // Does address point to LSB?
}

static SLuint32 s_getDefaultByteOrder() {
    return s_isLittleEndian() ? SL_BYTEORDER_LITTLEENDIAN : SL_BYTEORDER_BIGENDIAN;
}

Result AudioStreamOpenSLES::open() {

    SLresult result;

    __android_log_print(ANDROID_LOG_INFO, TAG, "AudioPlayerOpenSLES::Open(chans:%d, rate:%d)",
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

    SLuint32 bitsPerSample = getBytesPerSample() * OBOE_BITS_PER_BYTE;

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,    // locatorType
            static_cast<SLuint32>(mBurstsPerBuffer)};   // numBuffers

    // Define the audio data format.
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,       // formatType
            (SLuint32) mChannelCount,           // numChannels
            (SLuint32) (mSampleRate * kMillisPerSecond),    // milliSamplesPerSec
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

    result = OpenSLOutputMixer::getInstance()->createAudioPlayer(&bqPlayerObject_, &audioSrc);
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

    mSharingMode = SharingMode::Shared;
    mBufferCapacityInFrames = mFramesPerBurst * mBurstsPerBuffer;

    return Result::OK;
}

Result AudioStreamOpenSLES::close() {
//    __android_log_write(ANDROID_LOG_INFO, TAG, "AudioStreamOpenSLES()");
    // TODO make sure callback is no longer being called
    if (bqPlayerObject_ != NULL) {
        (*bqPlayerObject_)->Destroy(bqPlayerObject_);
        bqPlayerObject_ = NULL;

        // invalidate any interfaces
        bqPlayerPlay_ = NULL;
        bq_ = NULL;
    }
    return Result::OK;
}

Result AudioStreamOpenSLES::setPlayState(SLuint32 newState)
{
    Result result = Result::OK;
    LOGD("AudioStreamOpenSLES(): setPlayState()");
    if (bqPlayerPlay_ == NULL) {
        return Result::ErrorInvalidState;
    }
    SLresult slResult = (*bqPlayerPlay_)->SetPlayState(bqPlayerPlay_, newState);
    if(SL_RESULT_SUCCESS != slResult) {
        LOGD("AudioStreamOpenSLES(): setPlayState() returned %s", getSLErrStr(slResult));
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Pausing);
    }
    return result;
}

Result AudioStreamOpenSLES::requestStart()
{
    LOGD("AudioStreamOpenSLES(): requestStart()");
    Result result = setPlayState(SL_PLAYSTATE_PLAYING);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        enqueueBuffer();
        setState(StreamState::Starting);
    }
    return result;
}


Result AudioStreamOpenSLES::requestPause() {
    LOGD("AudioStreamOpenSLES(): requestPause()");
    Result result = setPlayState(SL_PLAYSTATE_PAUSED);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Pausing);
    }
    return result;
}

Result AudioStreamOpenSLES::requestFlush() {
    LOGD("AudioStreamOpenSLES(): requestFlush()");
    if (bqPlayerPlay_ == NULL) {
        return Result::ErrorInvalidState;
    }
    return Result::ErrorUnimplemented; // TODO
}

Result AudioStreamOpenSLES::requestStop()
{
    LOGD("AudioStreamOpenSLES(): requestStop()");
    Result result = setPlayState(SL_PLAYSTATE_STOPPED);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Stopping);
    }
    return result;
}

Result AudioStreamOpenSLES::waitForStateChange(StreamState currentState,
                                                      StreamState *nextState,
                                                      int64_t timeoutNanoseconds)
{
    LOGD("AudioStreamOpenSLES(): waitForStateChange()");
    if (bqPlayerPlay_ == NULL) {
        return Result::ErrorInvalidState;
    }
    return Result::ErrorUnimplemented; // TODO
}

int32_t AudioStreamOpenSLES::getFramesPerBurst() {
    return mFramesPerBurst;
}
