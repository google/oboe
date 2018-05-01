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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "aaudio/AAudioLoader.h"
#include "aaudio/AudioStreamAAudio.h"
#include "common/OboeDebug.h"
#include "oboe/Utilities.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

using namespace oboe;
AAudioLoader *AudioStreamAAudio::mLibLoader = nullptr;

// 'C' wrapper for the data callback method
static aaudio_data_callback_result_t oboe_aaudio_data_callback_proc(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {

    AudioStreamAAudio *oboeStream = (AudioStreamAAudio *)userData;
    if (oboeStream != NULL) {
        return static_cast<aaudio_data_callback_result_t>(
                oboeStream->callOnAudioReady(stream, audioData, numFrames));
    } else {
        return static_cast<aaudio_data_callback_result_t>(DataCallbackResult::Stop);
    }
}

static void oboe_aaudio_error_thread_proc(AudioStreamAAudio *oboeStream,
                                          AAudioStream *stream,
                                          Result error) {
    if (oboeStream != NULL) {
        oboeStream->onErrorInThread(stream, error);
    }
}

// 'C' wrapper for the error callback method
static void oboe_aaudio_error_callback_proc(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error) {

    AudioStreamAAudio *oboeStream = (AudioStreamAAudio *)userData;
    if (oboeStream != NULL) {
        // Handle error on a separate thread
        std::thread t(oboe_aaudio_error_thread_proc, oboeStream, stream, static_cast<Result>(error));
        t.detach();
    }
}

namespace oboe {

/*
 * Create a stream that uses Oboe Audio API.
 */
AudioStreamAAudio::AudioStreamAAudio(const AudioStreamBuilder &builder)
    : AudioStream(builder)
    , mFloatCallbackBuffer(nullptr)
    , mShortCallbackBuffer(nullptr)
    , mAAudioStream(nullptr) {
    mCallbackThreadEnabled.store(false);
    LOGD("AudioStreamAAudio() call isSupported()");
    isSupported();
}

AudioStreamAAudio::~AudioStreamAAudio() {
    delete[] mFloatCallbackBuffer;
    delete[] mShortCallbackBuffer;
}

bool AudioStreamAAudio::isSupported() {
    mLibLoader = AAudioLoader::getInstance();
    int openResult = mLibLoader->open();
    return openResult == 0;
}

Result AudioStreamAAudio::open() {
    Result result = Result::OK;

    if (mAAudioStream != nullptr) {
        return Result::ErrorInvalidState;
    }

    result = AudioStream::open();
    if (result != Result::OK) {
        return result;
    }

    LOGD("AudioStreamAAudio():  AAudio_createStreamBuilder()");
    AAudioStreamBuilder *aaudioBuilder;
    result = static_cast<Result>(mLibLoader->createStreamBuilder(&aaudioBuilder));
    if (result != Result::OK) {
        return result;
    }

    LOGD("AudioStreamAAudio.open() try with deviceId = %d", (int) mDeviceId);
    mLibLoader->builder_setBufferCapacityInFrames(aaudioBuilder, mBufferCapacityInFrames);
    mLibLoader->builder_setChannelCount(aaudioBuilder, mChannelCount);
    mLibLoader->builder_setDeviceId(aaudioBuilder, mDeviceId);
    mLibLoader->builder_setDirection(aaudioBuilder, static_cast<aaudio_direction_t>(mDirection));
    mLibLoader->builder_setFormat(aaudioBuilder, static_cast<aaudio_format_t>(mFormat));
    mLibLoader->builder_setSampleRate(aaudioBuilder, mSampleRate);
    mLibLoader->builder_setSharingMode(aaudioBuilder,
                                       static_cast<aaudio_sharing_mode_t>(mSharingMode));
    mLibLoader->builder_setPerformanceMode(aaudioBuilder,
                                           static_cast<aaudio_performance_mode_t>(mPerformanceMode));

    // TODO get more parameters from the builder?

    if (mStreamCallback != nullptr) {
        mLibLoader->builder_setDataCallback(aaudioBuilder, oboe_aaudio_data_callback_proc, this);
        mLibLoader->builder_setFramesPerDataCallback(aaudioBuilder, getFramesPerCallback());
    }
    mLibLoader->builder_setErrorCallback(aaudioBuilder, oboe_aaudio_error_callback_proc, this);

    {
        AAudioStream *stream = nullptr;
        result = static_cast<Result>(mLibLoader->builder_openStream(aaudioBuilder, &stream));
        mAAudioStream.store(stream);
    }
    if (result != Result::OK) {
        goto error2;
    }

    // Query and cache the values that will not change.
    mDeviceId = mLibLoader->stream_getDeviceId(mAAudioStream);
    mChannelCount = mLibLoader->stream_getChannelCount(mAAudioStream);
    mSampleRate = mLibLoader->stream_getSampleRate(mAAudioStream);
    mNativeFormat = static_cast<AudioFormat>(mLibLoader->stream_getFormat(mAAudioStream));
    if (mFormat == AudioFormat::Unspecified) {
        mFormat = mNativeFormat;
    }
    mSharingMode = static_cast<SharingMode>(mLibLoader->stream_getSharingMode(mAAudioStream));
    mPerformanceMode = static_cast<PerformanceMode>(
            mLibLoader->stream_getPerformanceMode(mAAudioStream));
    mBufferCapacityInFrames = mLibLoader->stream_getBufferCapacity(mAAudioStream);

    LOGD("AudioStreamAAudio.open() app    format = %d", (int) mFormat);
    LOGD("AudioStreamAAudio.open() native format = %d", (int) mNativeFormat);
    LOGD("AudioStreamAAudio.open() sample rate   = %d", (int) mSampleRate);
    LOGD("AudioStreamAAudio.open() capacity      = %d", (int) mBufferCapacityInFrames);

error2:
    mLibLoader->builder_delete(aaudioBuilder);
    LOGD("AudioStreamAAudio.open: AAudioStream_Open() returned %s, mAAudioStream = %p",
         mLibLoader->convertResultToText(static_cast<aaudio_result_t>(result)),
         mAAudioStream.load());
    return result;
}

Result AudioStreamAAudio::close()
{
    // The main reason we have this mutex if to prevent a collision between a call
    // by the application to stop a stream at the same time that an onError callback
    // is being executed because of a disconnect. The close will delete the stream,
    // which could otherwise cause the requestStop() to crash.
    std::lock_guard<std::mutex> lock(mLock);
    Result result = Result::OK;
    // This will delete the AAudio stream object so we need to null out the pointer.
    AAudioStream *stream = mAAudioStream.exchange(nullptr);
    if (stream != nullptr) {
        result = static_cast<Result>(mLibLoader->stream_close(stream));
    }
    return result;
}

DataCallbackResult AudioStreamAAudio::callOnAudioReady(AAudioStream *stream,
                                                                 void *audioData,
                                                                 int32_t numFrames) {
    return mStreamCallback->onAudioReady(
            this,
            audioData,
            numFrames);
}

void AudioStreamAAudio::onErrorInThread(AAudioStream *stream, Result error) {
    LOGD("onErrorInThread() - entering ===================================");
    assert(stream == mAAudioStream.load());
    requestStop();
    if (mStreamCallback != nullptr) {
        mStreamCallback->onErrorBeforeClose(this, error);
    }
    close();
    if (mStreamCallback != nullptr) {
        mStreamCallback->onErrorAfterClose(this, error);
    }
    LOGD("onErrorInThread() - exiting ===================================");
}

Result AudioStreamAAudio::convertApplicationDataToNative(int32_t numFrames) {
    Result result = Result::ErrorUnimplemented;
    int32_t numSamples = numFrames * getChannelCount();
    if (mFormat == AudioFormat::Float) {
        if (mNativeFormat == AudioFormat::I16) {
            convertFloatToPcm16(mFloatCallbackBuffer, mShortCallbackBuffer, numSamples);
            result = Result::OK;
        }
    } else if (mFormat == AudioFormat::I16) {
        if (mNativeFormat == AudioFormat::Float) {
            convertPcm16ToFloat(mShortCallbackBuffer, mFloatCallbackBuffer, numSamples);
            result = Result::OK;
        }
    }
    return result;
}

Result AudioStreamAAudio::requestStart() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_requestStart(stream));
    } else {
        return Result::ErrorNull;
    }
}

Result AudioStreamAAudio::requestPause() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_requestPause(stream));
    } else {
        return Result::ErrorNull;
    }
}

Result AudioStreamAAudio::requestFlush() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_requestFlush(stream));
    } else {
        return Result::ErrorNull;
    }
}

Result AudioStreamAAudio::requestStop() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_requestStop(stream));
    } else {
        return Result::ErrorNull;
    }
}

ErrorOrValue<int32_t>   AudioStreamAAudio::write(const void *buffer,
                                     int32_t numFrames,
                                     int64_t timeoutNanoseconds) {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t result = mLibLoader->stream_write(mAAudioStream, buffer,
                                                  numFrames, timeoutNanoseconds);
        if (result < 0) {
            return ErrorOrValue<int32_t>(static_cast<Result>(result));
        } else {
            return ErrorOrValue<int32_t>(result);
        }
    } else {
        return ErrorOrValue<int32_t>(Result::ErrorNull);
    }
}

ErrorOrValue<int32_t>   AudioStreamAAudio::read(void *buffer,
                                 int32_t numFrames,
                                 int64_t timeoutNanoseconds) {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t result = mLibLoader->stream_read(mAAudioStream, buffer,
                                                 numFrames, timeoutNanoseconds);
        if (result < 0) {
            return ErrorOrValue<int32_t>(static_cast<Result>(result));
        } else {
            return ErrorOrValue<int32_t>(result);
        }
    } else {
        return ErrorOrValue<int32_t>(Result::ErrorNull);
    }
}

Result AudioStreamAAudio::waitForStateChange(StreamState currentState,
                                        StreamState *nextState,
                                        int64_t timeoutNanoseconds) {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {

        aaudio_stream_state_t aaudioNextState;
        aaudio_result_t result = mLibLoader->stream_waitForStateChange(
                        mAAudioStream,
                        static_cast<aaudio_stream_state_t>(currentState),
                        &aaudioNextState,
                        timeoutNanoseconds);
        *nextState = static_cast<StreamState>(aaudioNextState);
        return static_cast<Result>(result);
    } else {
        return Result::ErrorNull;
    }
}

Result AudioStreamAAudio::setBufferSizeInFrames(int32_t requestedFrames) {
    if (requestedFrames > mBufferCapacityInFrames) {
        requestedFrames = mBufferCapacityInFrames;
    }
    return static_cast<Result>(mLibLoader->stream_setBufferSize(mAAudioStream, requestedFrames));
}

StreamState AudioStreamAAudio::getState() {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<StreamState>(mLibLoader->stream_getState(stream));
    } else {
        return StreamState::Closed;
    }
}

int32_t AudioStreamAAudio::getBufferSizeInFrames() const {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return mLibLoader->stream_getBufferSize(stream);
    } else {
        return static_cast<int32_t>(Result::ErrorNull);
    }
}

int32_t AudioStreamAAudio::getFramesPerBurst() {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return mLibLoader->stream_getFramesPerBurst(stream);
    } else {
        return static_cast<int32_t>(Result::ErrorNull);
    }
}

int64_t AudioStreamAAudio::getFramesRead() const {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return mLibLoader->stream_getFramesRead(stream);
    } else {
        return static_cast<int32_t>(Result::ErrorNull);
    }
}

int64_t AudioStreamAAudio::getFramesWritten() const {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return mLibLoader->stream_getFramesWritten(stream);
    } else {
        return static_cast<int64_t>(Result::ErrorNull);
    }
}

int32_t AudioStreamAAudio::getXRunCount() const {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return mLibLoader->stream_getXRunCount(stream);
    } else {
        return static_cast<int32_t>(Result::ErrorNull);
    }
}

Result AudioStreamAAudio::getTimestamp(clockid_t clockId,
                                   int64_t *framePosition,
                                   int64_t *timeNanoseconds) {
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_getTimestamp(stream, clockId,
                                               framePosition, timeNanoseconds));
    } else {
        return Result::ErrorNull;
    }
}

ErrorOrValue<double> AudioStreamAAudio::calculateLatencyMillis() {
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ErrorOrValue<double>(Result::ErrorNull);
    }

    // Get the time that a known audio frame was presented.
    int64_t hardwareFrameIndex;
    int64_t hardwareFrameHardwareTime;
    auto result = getTimestamp(CLOCK_MONOTONIC,
                               &hardwareFrameIndex,
                               &hardwareFrameHardwareTime);
    if (result != oboe::Result::OK) {
        return ErrorOrValue<double>(static_cast<Result>(result));
    }

    // Get counter closest to the app.
    bool isOutput = (getDirection() == oboe::Direction::Output);
    int64_t appFrameIndex = isOutput ? getFramesWritten() : getFramesRead();

    // Assume that the next frame will be processed at the current time
    using namespace std::chrono;
    int64_t appFrameAppTime =
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();

    // Calculate the number of frames between app and hardware
    int64_t frameIndexDelta = appFrameIndex - hardwareFrameIndex;

    // Calculate the time which the next frame will be or was presented
    int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) / getSampleRate();
    int64_t appFrameHardwareTime = hardwareFrameHardwareTime + frameTimeDelta;

    // The current latency is the difference in time between when the current frame is at
    // the app and when it is at the hardware.
    double latencyNanos = (double)((isOutput)
                          ? (appFrameHardwareTime - appFrameAppTime) // hardware is later
                          : (appFrameAppTime - appFrameHardwareTime)); // hardware is earlier
    double latencyMillis = latencyNanos / kNanosPerMillisecond;

    return ErrorOrValue<double>(latencyMillis);
}

} // namespace oboe
