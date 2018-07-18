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
            (SLuint32) mChannelCount,           // numChannels
            (SLuint32) (mSampleRate * kMillisPerSecond),    // milliSamplesPerSec
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
#if __ANDROID_API__ >= __ANDROID_API_L__
    SLAndroidDataFormat_PCM_EX format_pcm_ex;
    if (getSdkVersion() >= __ANDROID_API_L__) {
        SLuint32 representation = OpenSLES_ConvertFormatToRepresentation(getFormat());
        // Fill in the format structure.
        format_pcm_ex = OpenSLES_createExtendedFormat(format_pcm, representation);
        // Use in place of the previous format.
        audioSrc.pFormat = &format_pcm_ex;
    }
#endif // __ANDROID_API_L__

    result = OutputMixerOpenSL::getInstance().createAudioPlayer(&mObjectInterface,
                                                                          &audioSrc);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("createAudioPlayer() result:%s", getSLErrStr(result));
        goto error;
    }

    // Configure the stream.
    SLAndroidConfigurationItf configItf;
    result = (*mObjectInterface)->GetInterface(mObjectInterface,
                                               SL_IID_ANDROIDCONFIGURATION,
                                               &configItf);
    if (SL_RESULT_SUCCESS == result) {
        result = configurePerformanceMode(configItf);
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

    SLresult slResult = (*mPlayInterface)->SetPlayState(mPlayInterface, newState);
    if (SL_RESULT_SUCCESS != slResult) {
        LOGD("AudioOutputStreamOpenSLES(): setPlayState() returned %s", getSLErrStr(slResult));
        result = Result::ErrorInternal; // TODO convert slResult to Result::Error
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestStart() {

    LOGD("AudioOutputStreamOpenSLES(): requestStart()");
    if (mState == StreamState::Closed) return Result::ErrorClosed;

    setState(StreamState::Starting);
    Result result = setPlayState(SL_PLAYSTATE_PLAYING);
    if (result == Result::OK) {
        setState(StreamState::Started);
        processBufferCallback(mSimpleBufferQueueInterface);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestPause() {

    LOGD("AudioOutputStreamOpenSLES(): requestPause()");
    if (mState == StreamState::Closed) return Result::ErrorClosed;

    setState(StreamState::Pausing);
    Result result = setPlayState(SL_PLAYSTATE_PAUSED);
    if (result == Result::OK) {
        // Note that OpenSL ES does NOT reset its millisecond position when OUTPUT is paused.
        int64_t framesWritten = getFramesWritten();
        if (framesWritten >= 0) {
            setFramesRead(framesWritten);
        }
        setState(StreamState::Paused);
    }
    return result;
}

Result AudioOutputStreamOpenSLES::requestFlush() {

    LOGD("AudioOutputStreamOpenSLES(): requestFlush()");
    if (mState == StreamState::Closed) return Result::ErrorClosed;

    if (mPlayInterface == NULL) {
        return Result::ErrorInvalidState;
    }
    return Result::ErrorUnimplemented; // TODO
}

Result AudioOutputStreamOpenSLES::requestStop() {

    LOGD("AudioOutputStreamOpenSLES(): requestStop()");
    if (mState == StreamState::Closed) return Result::ErrorClosed;

    setState(StreamState::Stopping);

    Result result = setPlayState(SL_PLAYSTATE_STOPPED);
    if (result == Result::OK) {

        mPositionMillis.reset32(); // OpenSL ES resets its millisecond position when stopped.
        int64_t framesWritten = getFramesWritten();
        if (framesWritten >= 0) {
            setFramesRead(framesWritten);
        }
        setState(StreamState::Stopped);
    }
    return result;

}

void AudioOutputStreamOpenSLES::setFramesRead(int64_t framesRead) {
    int64_t millisWritten = framesRead * kMillisPerSecond / getSampleRate();
    mPositionMillis.set(millisWritten);
}

int64_t AudioOutputStreamOpenSLES::getFramesRead() {
    return getFramesProcessedByServer();
}

Result AudioOutputStreamOpenSLES::waitForStateChange(StreamState currentState,
                                               StreamState *nextState,
                                               int64_t timeoutNanoseconds) {
    LOGD("AudioOutputStreamOpenSLES::waitForStateChange()");

    if (getState() == StreamState::Closed){
        return Result::ErrorClosed;
    } else if (mPlayInterface == NULL) {
        return Result::ErrorInvalidState;
    }

    int64_t durationNanos = 20 * kNanosPerMillisecond; // arbitrary
    StreamState state = getState();

    while (state == currentState && timeoutNanoseconds > 0){
        if (durationNanos > timeoutNanoseconds){
            durationNanos = timeoutNanoseconds;
        }
        AudioClock::sleepForNanos(durationNanos);
        timeoutNanoseconds -= durationNanos;

        state = getState();
    }
    if (nextState != nullptr) {
        *nextState = state;
    }

    return (state == currentState) ? Result::ErrorTimeout : Result::OK;
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
