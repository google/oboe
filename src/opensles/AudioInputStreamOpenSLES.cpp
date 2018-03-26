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
#include "AudioInputStreamOpenSLES.h"
#include "AudioStreamOpenSLES.h"
#include "OpenSLESUtilities.h"

using namespace oboe;

AudioInputStreamOpenSLES::AudioInputStreamOpenSLES(const AudioStreamBuilder &builder)
        : AudioStreamOpenSLES(builder) {
}

AudioInputStreamOpenSLES::~AudioInputStreamOpenSLES() {
}

#define AUDIO_CHANNEL_COUNT_MAX         30u
#define SL_ANDROID_UNKNOWN_CHANNELMASK  0

int AudioInputStreamOpenSLES::chanCountToChanMask(int channelCount) {
    // from internal sles_channel_in_mask_from_count(chanCount);
    switch (channelCount) {
        case 1:
            return SL_SPEAKER_FRONT_LEFT;
        case 2:
            return SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        default: {
            if (channelCount > AUDIO_CHANNEL_COUNT_MAX) {
                return SL_ANDROID_UNKNOWN_CHANNELMASK;
            } else {
                SLuint32 bitfield = (1 << channelCount) - 1;
                return SL_ANDROID_MAKE_INDEXED_CHANNEL_MASK(bitfield);
            }
        }
    }
}

Result AudioInputStreamOpenSLES::open() {

    Result oboeResult = AudioStreamOpenSLES::open();
    if (Result::OK != oboeResult)  return oboeResult;

    SLuint32 bitsPerSample = getBytesPerSample() * kBitsPerByte;

    // configure audio sink
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,    // locatorType
            static_cast<SLuint32>(kBufferQueueLength)};   // numBuffers

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

    SLDataSink audioSink = {&loc_bufq, &format_pcm};

    /**
     * API 21 (Lollipop) introduced support for floating-point data representation and an extended
     * data format type: SLAndroidDataFormat_PCM_EX. If running on API 21+ use this newer format
     * type, creating it from our original format.
     */
    SLAndroidDataFormat_PCM_EX format_pcm_ex;
    if (__ANDROID_API__ >= __ANDROID_API_L__) {
        SLuint32 representation = OpenSLES_ConvertFormatToRepresentation(getFormat());
        // Fill in the format structure.
        format_pcm_ex = OpenSLES_createExtendedFormat(format_pcm, representation);
        // Use in place of the previous format.
        audioSink.pFormat = &format_pcm_ex;
    }

    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                      SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,
                                      NULL };
    SLDataSource audioSrc = {&loc_dev, NULL };

    SLresult result = EngineOpenSLES::getInstance().createAudioRecorder(&mObjectInterface,
                                                                       &audioSrc,
                                                                       &audioSink);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("createAudioRecorder() result:%s", getSLErrStr(result));
        goto error;
    }

    // Configure the voice recognition preset, which has no
    // signal processing, for lower latency.
    SLAndroidConfigurationItf inputConfig;
    result = (*mObjectInterface)->GetInterface(mObjectInterface,
                                            SL_IID_ANDROIDCONFIGURATION,
                                            &inputConfig);
    if (SL_RESULT_SUCCESS == result) {
        SLuint32 presetValue = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
        (*inputConfig)->SetConfiguration(inputConfig,
                                         SL_ANDROID_KEY_RECORDING_PRESET,
                                         &presetValue,
                                         sizeof(SLuint32));
    }

    result = (*mObjectInterface)->Realize(mObjectInterface, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("Realize recorder object result:%s", getSLErrStr(result));
        goto error;
    }

    result = (*mObjectInterface)->GetInterface(mObjectInterface, SL_IID_RECORD, &mRecordInterface);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("GetInterface RECORD result:%s", getSLErrStr(result));
        goto error;
    }

    result = AudioStreamOpenSLES::registerBufferQueueCallback();
    if (SL_RESULT_SUCCESS != result) {
        goto error;
    }

    allocateFifo();

    return Result::OK;

error:
    return Result::ErrorInternal; // TODO convert error from SLES to OBOE
}

Result AudioInputStreamOpenSLES::close() {
    requestStop();
    mRecordInterface = NULL;
    return AudioStreamOpenSLES::close();
}

Result AudioInputStreamOpenSLES::setRecordState(SLuint32 newState) {
    Result result = Result::OK;
    LOGD("AudioInputStreamOpenSLES::setRecordState(%d)", newState);
    if (mRecordInterface == nullptr) {
        LOGE("AudioInputStreamOpenSLES::SetRecordState() mRecordInterface is null");
        return Result::ErrorInvalidState;
    }
    SLresult slResult = (*mRecordInterface)->SetRecordState(mRecordInterface, newState);
    if(SL_RESULT_SUCCESS != slResult) {
        LOGE("AudioInputStreamOpenSLES::SetRecordState() returned %s", getSLErrStr(slResult));
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Pausing);
    }
    return result;
}

Result AudioInputStreamOpenSLES::requestStart()
{
    LOGD("AudioInputStreamOpenSLES::requestStart()");
    Result result = setRecordState(SL_RECORDSTATE_RECORDING);
    if(result == Result::OK) {
        // Enqueue the first buffer so that we have data ready in the callback.
        enqueueCallbackBuffer(mSimpleBufferQueueInterface);
        setState(StreamState::Starting);
    }
    return result;
}


Result AudioInputStreamOpenSLES::requestPause() {
    LOGD("AudioInputStreamOpenSLES::requestStop()");
    Result result = setRecordState(SL_RECORDSTATE_PAUSED);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Pausing);
    }
    return result;
}

Result AudioInputStreamOpenSLES::requestFlush() {
    return Result::ErrorUnimplemented; // TODO
}

Result AudioInputStreamOpenSLES::requestStop() {
    LOGD("AudioInputStreamOpenSLES::requestStop()");
    Result result = setRecordState(SL_RECORDSTATE_STOPPED);
    if(result != Result::OK) {
        result = Result::ErrorInvalidState; // TODO review
    } else {
        setState(StreamState::Stopping);
    }
    return result;
}

Result AudioInputStreamOpenSLES::waitForStateChange(StreamState currentState,
                                                     StreamState *nextState,
                                                     int64_t timeoutNanoseconds) {
    LOGD("AudioInputStreamOpenSLES::waitForStateChange()");
    if (mRecordInterface == NULL) {
        return Result::ErrorInvalidState;
    }
    return Result::ErrorUnimplemented; // TODO
}
