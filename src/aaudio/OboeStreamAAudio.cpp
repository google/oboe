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

#include <stdint.h>

#include "oboe/OboeUtilities.h"
#include "common/OboeDebug.h"
#include "aaudio/AAudioLoader.h"
#include "aaudio/OboeStreamAAudio.h"

AAudioLoader *OboeStreamAAudio::mLibLoader = nullptr;

/*
 * Create a stream that uses Oboe Audio API.
 */
OboeStreamAAudio::OboeStreamAAudio(const OboeStreamBuilder &builder)
    : OboeStream(builder)
    , mFloatCallbackBuffer(nullptr)
    , mShortCallbackBuffer(nullptr)
    , mAAudioStream(nullptr)
{
    mCallbackThreadEnabled.store(false);
    LOGD("OboeStreamAAudio() call isSupported()");
    isSupported();
}

OboeStreamAAudio::~OboeStreamAAudio()
{
    delete[] mFloatCallbackBuffer;
    delete[] mShortCallbackBuffer;
}

bool OboeStreamAAudio::isSupported() {
    mLibLoader = AAudioLoader::getInstance();
    int openResult = mLibLoader->open();
    return openResult == 0;
}

// 'C' wrapper for the data callback method
static aaudio_data_callback_result_t oboe_aaudio_data_callback_proc(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {

    OboeStreamAAudio *oboeStream = (OboeStreamAAudio *)userData;
    if (oboeStream != NULL) {
        return oboeStream->callOnAudioReady(stream, audioData, numFrames);
    } else {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }
}

// 'C' wrapper for the error callback method
static void oboe_aaudio_error_callback_proc(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error) {

    OboeStreamAAudio *oboeStream = (OboeStreamAAudio *)userData;
    if (oboeStream != NULL) {
        oboeStream->callOnError(stream, error);
    }
}

oboe_result_t OboeStreamAAudio::open() {
    oboe_result_t result = AAUDIO_OK;

    if (mAAudioStream != nullptr) {
        return AAUDIO_ERROR_INVALID_STATE;
    }

    result = OboeStream::open();
    if (result != AAUDIO_OK) {
        return result;
    }

    LOGD("OboeStreamAAudio():  AAudio_createStreamBuilder()");
    AAudioStreamBuilder *aaudioBuilder;
    result = mLibLoader->createStreamBuilder(&aaudioBuilder);
    if (result != AAUDIO_OK) {
        return result;
    }

    LOGD("OboeStreamAAudio.open() try with deviceId = %d", (int) mDeviceId);
    mLibLoader->builder_setBufferCapacityInFrames(aaudioBuilder, mBufferCapacityInFrames);
    mLibLoader->builder_setChannelCount(aaudioBuilder, mChannelCount);
    mLibLoader->builder_setDeviceId(aaudioBuilder, mDeviceId);
    mLibLoader->builder_setDirection(aaudioBuilder, mDirection);
    mLibLoader->builder_setFormat(aaudioBuilder, mFormat);
    mLibLoader->builder_setSampleRate(aaudioBuilder, mSampleRate);
    mLibLoader->builder_setSharingMode(aaudioBuilder, mSharingMode);
    mLibLoader->builder_setPerformanceMode(aaudioBuilder, mPerformanceMode);

    // TODO get more parameters from the builder

    if (mStreamCallback != nullptr) {
        mLibLoader->builder_setDataCallback(aaudioBuilder, oboe_aaudio_data_callback_proc, this);
        mLibLoader->builder_setErrorCallback(aaudioBuilder, oboe_aaudio_error_callback_proc, this);
        mLibLoader->builder_setFramesPerDataCallback(aaudioBuilder, getFramesPerCallback());
    }

    result = mLibLoader->builder_openStream(aaudioBuilder, &mAAudioStream);
    if (result != AAUDIO_OK) {
        goto error2;
    }

    // Query and cache the values that will not change.
    mDeviceId = mLibLoader->stream_getDeviceId(mAAudioStream);
    mChannelCount = mLibLoader->stream_getChannelCount(mAAudioStream);
    mSampleRate = mLibLoader->stream_getSampleRate(mAAudioStream);
    mNativeFormat = mLibLoader->stream_getFormat(mAAudioStream);
    if (mFormat == OBOE_AUDIO_FORMAT_UNSPECIFIED) {
        mFormat = mNativeFormat;
    }
    mSharingMode = mLibLoader->stream_getSharingMode(mAAudioStream);
    mPerformanceMode = mLibLoader->stream_getPerformanceMode(mAAudioStream);
    mBufferCapacityInFrames = getBufferCapacityInFrames();

    LOGD("OboeStreamAAudio.open() app    format = %d", (int) mFormat);
    LOGD("OboeStreamAAudio.open() native format = %d", (int) mNativeFormat);
    LOGD("OboeStreamAAudio.open() sample rate   = %d", (int) mSampleRate);

error2:
    mLibLoader->builder_delete(aaudioBuilder);
    LOGD("OboeStreamAAudio.open: AAudioStream_Open() returned %s, mAAudioStream = %p",
         mLibLoader->convertResultToText(result), mAAudioStream);
    return result;
}

oboe_result_t OboeStreamAAudio::close()
{
    oboe_result_t result = mLibLoader->stream_close(mAAudioStream);
    mAAudioStream = nullptr;
    return result;
}

aaudio_data_callback_result_t OboeStreamAAudio::callOnAudioReady(AAudioStream *stream,
                                                                 void *audioData,
                                                                 int32_t numFrames) {
    return mStreamCallback->onAudioReady(
            this,
            audioData,
            numFrames);
}

void OboeStreamAAudio::callOnError(AAudioStream *stream, oboe_result_t error) {
    mStreamCallback->onError( this, error);
}

oboe_result_t OboeStreamAAudio::convertApplicationDataToNative(int32_t numFrames) {
    oboe_result_t result = OBOE_ERROR_UNIMPLEMENTED;
    int32_t numSamples = numFrames * getChannelCount();
    if (mFormat == OBOE_AUDIO_FORMAT_PCM_FLOAT) {
        if (mNativeFormat == OBOE_AUDIO_FORMAT_PCM_I16) {
            Oboe_convertFloatToPcm16(mFloatCallbackBuffer, mShortCallbackBuffer, numSamples);
            result = AAUDIO_OK;
        }
    } else if (mFormat == OBOE_AUDIO_FORMAT_PCM_I16) {
        if (mNativeFormat == OBOE_AUDIO_FORMAT_PCM_FLOAT) {
            Oboe_convertPcm16ToFloat(mShortCallbackBuffer, mFloatCallbackBuffer, numSamples);
            result = AAUDIO_OK;
        }
    }
    return result;
}

oboe_result_t OboeStreamAAudio::requestStart()
{
    return mLibLoader->stream_requestStart(mAAudioStream);
}

oboe_result_t OboeStreamAAudio::requestPause()
{
    return mLibLoader->stream_requestPause(mAAudioStream);
}

oboe_result_t OboeStreamAAudio::requestFlush() {
    return mLibLoader->stream_requestFlush(mAAudioStream);
}

oboe_result_t OboeStreamAAudio::requestStop()
{
    return mLibLoader->stream_requestStop(mAAudioStream);
}

oboe_result_t OboeStreamAAudio::write(const void *buffer,
                                     int32_t numFrames,
                                     int64_t timeoutNanoseconds)
{
    return mLibLoader->stream_write(mAAudioStream, buffer, numFrames, timeoutNanoseconds);
}

oboe_result_t OboeStreamAAudio::waitForStateChange(oboe_stream_state_t currentState,
                                                  oboe_stream_state_t *nextState,
                                                  int64_t timeoutNanoseconds)
{
    return mLibLoader->stream_waitForStateChange(mAAudioStream, currentState,
                                                 nextState, timeoutNanoseconds);
}

oboe_result_t OboeStreamAAudio::setBufferSizeInFrames(int32_t requestedFrames)
{
    return mLibLoader->stream_setBufferSize(mAAudioStream, requestedFrames);
}

oboe_stream_state_t OboeStreamAAudio::getState()
{
    if (mAAudioStream == nullptr) {
        return OBOE_STREAM_STATE_CLOSED;
    }
    return mLibLoader->stream_getState(mAAudioStream);
}

int32_t OboeStreamAAudio::getBufferSizeInFrames() const {
    return mLibLoader->stream_getBufferSize(mAAudioStream);
}

int32_t OboeStreamAAudio::getBufferCapacityInFrames() const {
    return mLibLoader->stream_getBufferCapacity(mAAudioStream);
}

int32_t OboeStreamAAudio::getFramesPerBurst()
{
    return mLibLoader->stream_getFramesPerBurst(mAAudioStream);
}

int64_t OboeStreamAAudio::getFramesRead()
{
    return mLibLoader->stream_getFramesRead(mAAudioStream);
}
int64_t OboeStreamAAudio::getFramesWritten()
{
    return mLibLoader->stream_getFramesWritten(mAAudioStream);
}

int32_t OboeStreamAAudio::getXRunCount()
{
    return mLibLoader->stream_getXRunCount(mAAudioStream);
}

oboe_result_t OboeStreamAAudio::getTimestamp(clockid_t clockId,
                                   int64_t *framePosition,
                                   int64_t *timeNanoseconds) {
    return mLibLoader->stream_getTimestamp(mAAudioStream, clockId,
                                   framePosition, timeNanoseconds);
}
