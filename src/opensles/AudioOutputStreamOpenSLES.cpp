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
#include <common/AudioClock.h>

#include "oboe/AudioStreamBuilder.h"
#include "AudioOutputStreamOpenSLES.h"
#include "AudioStreamOpenSLES.h"
#include "OpenSLESUtilities.h"
#include "OutputMixerOpenSLES.h"

using namespace oboe;

static SLuint32 OpenSLES_convertOutputUsage(Usage oboeUsage) {
    SLuint32 openslStream = SL_ANDROID_STREAM_MEDIA;
    switch(oboeUsage) {
        case Usage::Media:
            openslStream = SL_ANDROID_STREAM_MEDIA;
            break;
        case Usage::VoiceCommunication:
        case Usage::VoiceCommunicationSignalling:
            openslStream = SL_ANDROID_STREAM_VOICE;
            break;
        case Usage::Alarm:
            openslStream = SL_ANDROID_STREAM_ALARM;
            break;
        case Usage::Notification:
        case Usage::NotificationRingtone:
        case Usage::NotificationEvent:
            openslStream = SL_ANDROID_STREAM_NOTIFICATION;
            break;
        case Usage::AssistanceAccessibility:
        case Usage::AssistanceNavigationGuidance:
        case Usage::AssistanceSonification:
            openslStream = SL_ANDROID_STREAM_SYSTEM;
            break;
        case Usage::Game:
            openslStream = SL_ANDROID_STREAM_MEDIA;
            break;
        case Usage::Assistant:
        default:
            openslStream = SL_ANDROID_STREAM_SYSTEM;
            break;
    }
    return openslStream;
}

AudioOutputStreamOpenSLES::AudioOutputStreamOpenSLES(const AudioStreamBuilder &builder)
        : AudioStreamOpenSLES(builder) {
}

AudioOutputStreamOpenSLES::~AudioOutputStreamOpenSLES() {
}

// These will wind up in <SLES/OpenSLES_Android.h>
constexpr int SL_ANDROID_SPEAKER_STEREO = (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);

constexpr int SL_ANDROID_SPEAKER_QUAD = (SL_ANDROID_SPEAKER_STEREO
        | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT);

constexpr int SL_ANDROID_SPEAKER_5DOT1 = (SL_ANDROID_SPEAKER_QUAD
        | SL_SPEAKER_FRONT_CENTER  | SL_SPEAKER_LOW_FREQUENCY);

constexpr int SL_ANDROID_SPEAKER_7DOT1 = (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT
        | SL_SPEAKER_SIDE_RIGHT);

SLuint32 AudioOutputStreamOpenSLES::channelCountToChannelMask(int channelCount) {
    SLuint32 channelMask = 0;

    switch (channelCount) {
        case  1:
            channelMask = SL_SPEAKER_FRONT_CENTER;
            break;

        case  2:
            channelMask = SL_ANDROID_SPEAKER_STEREO;
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

        default:
            channelMask = channelCountToChannelMaskDefault(channelCount);
            break;
    }
    return channelMask;
}

Result AudioOutputStreamOpenSLES::open() {
    SLAndroidConfigurationItf configItf = nullptr;


    if (getSdkVersion() < __ANDROID_API_L__ && mFormat == AudioFormat::Float){
        // TODO: Allow floating point format on API <21 using float->int16 converter
        return Result::ErrorInvalidFormat;
    }

    // If audio format is unspecified then choose a suitable default.
    // API 21+: FLOAT
    // API <21: INT16
    if (mFormat == AudioFormat::Unspecified){
        mFormat = (getSdkVersion() < __ANDROID_API_L__) ?
                  AudioFormat::I16 : AudioFormat::Float;
    }

    Result oboeResult = AudioStreamOpenSLES::open();
    if (Result::OK != oboeResult)  return oboeResult;

    SLresult result = OutputMixerOpenSL::getInstance().open();
    if (SL_RESULT_SUCCESS != result) {
        AudioStreamOpenSLES::close();
        return Result::ErrorInternal;
    }

    SLuint32 bitsPerSample = getBytesPerSample() * kBitsPerByte;

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,    // locatorType
            static_cast<SLuint32>(kBufferQueueLength)};   // numBuffers

    // Define the audio data format.
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,       // formatType
            static_cast<SLuint32>(mChannelCount),           // numChannels
            static_cast<SLuint32>(mSampleRate * kMillisPerSecond),    // milliSamplesPerSec
            bitsPerSample,                      // bitsPerSample
            bitsPerSample,                      // containerSize;
            channelCountToChannelMask(mChannelCount), // channelMask
            getDefaultByteOrder(),
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    /**
     * API 21 (Lollipop) introduced support for floating-point data representation and an extended
     * data format type: SLAndroidDataFormat_PCM_EX. If running on API 21+ use this newer format
     * type, creating it from our original format.
     */
    SLAndroidDataFormat_PCM_EX format_pcm_ex;
    if (getSdkVersion() >= __ANDROID_API_L__) {
        SLuint32 representation = OpenSLES_ConvertFormatToRepresentation(getFormat());
        // Fill in the format structure.
        format_pcm_ex = OpenSLES_createExtendedFormat(format_pcm, representation);
        // Use in place of the previous format.
        audioSrc.pFormat = &format_pcm_ex;
    }

    result = OutputMixerOpenSL::getInstance().createAudioPlayer(&mObjectInterface,
                                                                          &audioSrc);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("createAudioPlayer() result:%s", getSLErrStr(result));
        goto error;
    }

    // Configure the stream.
    result = (*mObjectInterface)->GetInterface(mObjectInterface,
                                               SL_IID_ANDROIDCONFIGURATION,
                                               (void *)&configItf);
    if (SL_RESULT_SUCCESS != result) {
        LOGW("%s() GetInterface(SL_IID_ANDROIDCONFIGURATION) failed with %s",
             __func__, getSLErrStr(result));
    } else {
        result = configurePerformanceMode(configItf);
        if (SL_RESULT_SUCCESS != result) {
            goto error;
        }

        SLuint32 presetValue = OpenSLES_convertOutputUsage(getUsage());
        result = (*configItf)->SetConfiguration(configItf,
                                                SL_ANDROID_KEY_STREAM_TYPE,
                                                &presetValue,
                                                sizeof(SLuint32));
        if (SL_RESULT_SUCCESS != result) {
            goto error;
        }
    }

    result = (*mObjectInterface)->Realize(mObjectInterface, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("Realize player object result:%s", getSLErrStr(result));
        goto error;
    }

    result = (*mObjectInterface)->GetInterface(mObjectInterface, SL_IID_PLAY, &mPlayInterface);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("GetInterface PLAY result:%s", getSLErrStr(result));
        goto error;
    }

    result = AudioStreamOpenSLES::registerBufferQueueCallback();
    if (SL_RESULT_SUCCESS != result) {
        goto error;
    }

    result = updateStreamParameters(configItf);
    if (SL_RESULT_SUCCESS != result) {
        goto error;
    }

    allocateFifo();

    setState(StreamState::Open);
    return Result::OK;
error:
    return Result::ErrorInternal; // TODO convert error from SLES to OBOE
}

Result AudioOutputStreamOpenSLES::onAfterDestroy() {
    OutputMixerOpenSL::getInstance().close();

    return Result::OK;
}

Result AudioOutputStreamOpenSLES::close() {

    if (mState == StreamState::Closed){
        return Result::ErrorClosed;
    } else {
        requestPause();
        // invalidate any interfaces
        mPlayInterface = NULL;
        return AudioStreamOpenSLES::close();
    }
}

Result AudioOutputStreamOpenSLES::setPlayState(SLuint32 newState) {

    LOGD("AudioOutputStreamOpenSLES(): setPlayState()");
    Result result = Result::OK;

    if (mPlayInterface == nullptr){
        LOGE("AudioOutputStreamOpenSLES::setPlayState() mPlayInterface is null");
        return Result::ErrorInvalidState;
    }

    SLresult slResult = (*mPlayInterface)->SetPlayState(mPlayInterface, newState);
    if (SL_RESULT_SUCCESS != slResult) {
        LOGD("AudioOutputStreamOpenSLES(): setPlayState() returned %s", getSLErrStr(slResult));
        result = Result::ErrorInternal; // TODO convert slResult to Result::Error
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestStart() {

    LOGD("AudioOutputStreamOpenSLES(): requestStart()");
    StreamState initialState = getState();
    if (initialState == StreamState::Closed) return Result::ErrorClosed;

    setState(StreamState::Starting);
    Result result = setPlayState(SL_PLAYSTATE_PLAYING);
    if (result == Result::OK) {
        setState(StreamState::Started);
        processBufferCallback(mSimpleBufferQueueInterface);
    } else {
        setState(initialState);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestPause() {

    LOGD("AudioOutputStreamOpenSLES::requestPause()");
    StreamState initialState = getState();
    if (initialState == StreamState::Closed) return Result::ErrorClosed;

    setState(StreamState::Pausing);
    Result result = setPlayState(SL_PLAYSTATE_PAUSED);
    if (result == Result::OK) {
        // Note that OpenSL ES does NOT reset its millisecond position when OUTPUT is paused.
        int64_t framesWritten = getFramesWritten();
        if (framesWritten >= 0) {
            setFramesRead(framesWritten);
        }
        setState(StreamState::Paused);
    } else {
        setState(initialState);
    }
    return result;
}

/**
 * Flush/clear the queue buffers
 */
Result AudioOutputStreamOpenSLES::requestFlush() {

    LOGD("AudioOutputStreamOpenSLES(): requestFlush()");
    if (getState() == StreamState::Closed) return Result::ErrorClosed;

    if (mPlayInterface == NULL || mSimpleBufferQueueInterface == NULL) {
        return Result::ErrorInvalidState;
    } else {
        SLresult result = (*mSimpleBufferQueueInterface)->Clear(mSimpleBufferQueueInterface);
        if (result == SL_RESULT_SUCCESS){
            return Result::OK;
        } else {
            LOGW("Failed to clear buffer queue. OpenSLES error: %d", result);
            return Result::ErrorInternal;
        }
    }
}

Result AudioOutputStreamOpenSLES::requestStop() {

    LOGD("AudioOutputStreamOpenSLES(): requestStop()");
    StreamState initialState = getState();
    if (initialState == StreamState::Closed) return Result::ErrorClosed;

    setState(StreamState::Stopping);

    Result result = setPlayState(SL_PLAYSTATE_STOPPED);
    if (result == Result::OK) {

        // Also clear the buffer queue so the old data won't be played if the stream is restarted
        if (requestFlush() != Result::OK){
            LOGW("Failed to flush the stream. Error %s", convertToText(flush()));
        }

        mPositionMillis.reset32(); // OpenSL ES resets its millisecond position when stopped.
        int64_t framesWritten = getFramesWritten();
        if (framesWritten >= 0) {
            setFramesRead(framesWritten);
        }
        setState(StreamState::Stopped);
    } else {
        setState(initialState);
    }
    return result;

}

void AudioOutputStreamOpenSLES::setFramesRead(int64_t framesRead) {
    int64_t millisWritten = framesRead * kMillisPerSecond / getSampleRate();
    mPositionMillis.set(millisWritten);
}

void AudioOutputStreamOpenSLES::updateFramesRead() {
    if (usingFIFO()) {
        AudioStreamBuffered::updateFramesRead();
    } else {
        mFramesRead = getFramesProcessedByServer();
    }
}

Result AudioOutputStreamOpenSLES::updateServiceFrameCounter() {
    if (mPlayInterface == NULL) {
        return Result::ErrorNull;
    }
    SLmillisecond msec = 0;
    SLresult slResult = (*mPlayInterface)->GetPosition(mPlayInterface, &msec);
    Result result = Result::OK;
    if(SL_RESULT_SUCCESS != slResult) {
        LOGD("%s(): GetPosition() returned %s", __func__, getSLErrStr(slResult));
        // set result based on SLresult
        result = Result::ErrorInternal;
    } else {
        mPositionMillis.update32(msec);
    }
    return result;
}
