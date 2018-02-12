/*
 * Copyright 2017 The Android Open Source Project
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

#include <cassert>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "oboe/AudioStreamBuilder.h"
#include "AudioOutputStreamOpenSLES.h"
#include "AudioStreamOpenSLES.h"
#include "OpenSLESUtilities.h"

using namespace oboe;

#define OBOE_BITS_PER_BYTE            8      // common value

AudioOutputStreamOpenSLES::AudioOutputStreamOpenSLES(const AudioStreamBuilder &builder)
        : AudioStreamOpenSLES(builder) {
    OpenSLOutputMixer::getInstance()->open();
}

AudioOutputStreamOpenSLES::~AudioOutputStreamOpenSLES() {
    OpenSLOutputMixer::getInstance()->close();
}

// These will wind up in <SLES/OpenSLES_Android.h>
#define SL_ANDROID_SPEAKER_QUAD (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT \
 | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT)

#define SL_ANDROID_SPEAKER_5DOT1 (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT \
 | SL_SPEAKER_FRONT_CENTER  | SL_SPEAKER_LOW_FREQUENCY| SL_SPEAKER_BACK_LEFT \
 | SL_SPEAKER_BACK_RIGHT)

#define SL_ANDROID_SPEAKER_7DOT1 (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT \
 |SL_SPEAKER_SIDE_RIGHT)

int AudioOutputStreamOpenSLES::chanCountToChanMask(int chanCount) {
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

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    ((AudioStreamOpenSLES *) context)->enqueueBuffer();
}

Result AudioOutputStreamOpenSLES::open() {

    Result oboeResult = AudioStreamOpenSLES::open();
    if (Result::OK != oboeResult)  return oboeResult;

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
            getDefaultByteOrder(),
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    /**
     * API 21 (Lollipop) introduced support for floating-point data representation and an extended
     * data format type: SLAndroidDataFormat_PCM_EX. If running on API 21+ use this newer format
     * type, creating it from our original format.
     */
    if (__ANDROID_API__ >= __ANDROID_API_L__) {
        SLuint32 representation = OpenSLES_ConvertFormatToRepresentation(getFormat());
        SLAndroidDataFormat_PCM_EX format_pcm_ex = OpenSLES_createExtendedFormat(format_pcm,
                                                                                 representation);
        // Overwrite the previous format.
        audioSrc.pFormat = &format_pcm_ex;
    }

    SLresult result = OpenSLOutputMixer::getInstance()->createAudioPlayer(&bqPlayerObject_,
                                                                          &audioSrc);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("createAudioPlayer() result:%s", getSLErrStr(result));
        goto error;
    }

    result = (*bqPlayerObject_)->Realize(bqPlayerObject_, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("Realize player object result:%s", getSLErrStr(result));
        goto error;
    }

    result = (*bqPlayerObject_)->GetInterface(bqPlayerObject_, SL_IID_PLAY, &bqPlayerPlay_);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get player interface result:%s", getSLErrStr(result));
        goto error;
    }

    // The BufferQueue
    result = (*bqPlayerObject_)->GetInterface(bqPlayerObject_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                              &bq_);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get bufferqueue interface:%p result:%s", bq_, getSLErrStr(result));
        goto error;
    }

    // The register BufferQueue callback
    result = (*bq_)->RegisterCallback(bq_, bqPlayerCallback, this);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("register callback result:%s", getSLErrStr(result));
        goto error;
    }

    return Result::OK;
error:
    return Result::ErrorInternal; // TODO convert error from SLES to OBOE
}

Result AudioOutputStreamOpenSLES::close() {
    requestPause();
    if (bqPlayerObject_ != NULL) {
        (*bqPlayerObject_)->Destroy(bqPlayerObject_);
        bqPlayerObject_ = NULL;

        // invalidate any interfaces
        bqPlayerPlay_ = NULL;
    }
    return AudioStreamOpenSLES::close();
}

Result AudioOutputStreamOpenSLES::setPlayState(SLuint32 newState) {
    Result result = Result::OK;
    LOGD("AudioOutputStreamOpenSLES(): setPlayState()");
    if (bqPlayerPlay_ == NULL) {
        return Result::ErrorInvalidState;
    }
    SLresult slResult = (*bqPlayerPlay_)->SetPlayState(bqPlayerPlay_, newState);
    if(SL_RESULT_SUCCESS != slResult) {
        LOGD("AudioOutputStreamOpenSLES(): setPlayState() returned %s", getSLErrStr(slResult));
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Pausing);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestStart() {
    LOGD("AudioOutputStreamOpenSLES(): requestStart()");
    Result result = setPlayState(SL_PLAYSTATE_PLAYING);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        enqueueBuffer();
        setState(StreamState::Starting);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestPause() {
    LOGD("AudioOutputStreamOpenSLES(): requestPause()");
    Result result = setPlayState(SL_PLAYSTATE_PAUSED);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Pausing);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestFlush() {
    LOGD("AudioOutputStreamOpenSLES(): requestFlush()");
    if (bqPlayerPlay_ == NULL) {
        return Result::ErrorInvalidState;
    }
    return Result::ErrorUnimplemented; // TODO
}

Result AudioOutputStreamOpenSLES::requestStop() {
    LOGD("AudioOutputStreamOpenSLES(): requestStop()");
    Result result = setPlayState(SL_PLAYSTATE_STOPPED);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Stopping);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::waitForStateChange(StreamState currentState,
                                               StreamState *nextState,
                                               int64_t timeoutNanoseconds) {
    LOGD("AudioOutputStreamOpenSLES::waitForStateChange()");
    if (bqPlayerPlay_ == NULL) {
        return Result::ErrorInvalidState;
    }
    return Result::ErrorUnimplemented; // TODO
}
