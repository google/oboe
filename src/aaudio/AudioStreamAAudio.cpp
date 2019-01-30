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

    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(userData);
    if (oboeStream != nullptr) {
        return static_cast<aaudio_data_callback_result_t>(
                oboeStream->callOnAudioReady(stream, audioData, numFrames));

    } else {
        return static_cast<aaudio_data_callback_result_t>(DataCallbackResult::Stop);
    }
}

static void oboe_aaudio_error_thread_proc(AudioStreamAAudio *oboeStream,
                                          AAudioStream *stream,
                                          Result error) {
    if (oboeStream != nullptr) {
        oboeStream->onErrorInThread(stream, error);
    }
}

// 'C' wrapper for the error callback method
static void oboe_aaudio_error_callback_proc(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error) {

    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(userData);
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
    , mAAudioStream(nullptr) {
    mCallbackThreadEnabled.store(false);
    LOGD("AudioStreamAAudio() call isSupported()");
    isSupported();
}

bool AudioStreamAAudio::isSupported() {
    mLibLoader = AAudioLoader::getInstance();
    int openResult = mLibLoader->open();
    return openResult == 0;
}

Result AudioStreamAAudio::open() {
    std::lock_guard<std::mutex> lock(mLock); // prevent use before open finishes

    Result result = Result::OK;
    AAudioStream *stream = nullptr;


    if (mAAudioStream != nullptr) {
        return Result::ErrorInvalidState;
    }

    result = AudioStream::open();
    if (result != Result::OK) {
        return result;
    }

    AAudioStreamBuilder *aaudioBuilder;
    result = static_cast<Result>(mLibLoader->createStreamBuilder(&aaudioBuilder));
    if (result != Result::OK) {
        return result;
    }

    // Do not set INPUT capacity below 4096 because that prevents us from getting a FAST track
    // when using the Legacy data path.
    // If the app requests > 4096 then we allow it but we are less likely to get LowLatency.
    // See internal bug b/80308183 for more details.
    int32_t capacity = mBufferCapacityInFrames;
    constexpr int kCapacityRequiredForFastLegacyTrack = 4096; // matches value in AudioFinger
    if (mDirection == oboe::Direction::Input
            && capacity != oboe::Unspecified
            && capacity < kCapacityRequiredForFastLegacyTrack
            && mPerformanceMode == oboe::PerformanceMode::LowLatency) {
        capacity = kCapacityRequiredForFastLegacyTrack;
        LOGD("AudioStreamAAudio.open() capacity changed from %d to %d",
             static_cast<int>(mBufferCapacityInFrames), capacity);
    }
    mLibLoader->builder_setBufferCapacityInFrames(aaudioBuilder, capacity);

    mLibLoader->builder_setChannelCount(aaudioBuilder, mChannelCount);
    mLibLoader->builder_setDeviceId(aaudioBuilder, mDeviceId);
    mLibLoader->builder_setDirection(aaudioBuilder, static_cast<aaudio_direction_t>(mDirection));
    mLibLoader->builder_setFormat(aaudioBuilder, static_cast<aaudio_format_t>(mFormat));
    mLibLoader->builder_setSampleRate(aaudioBuilder, mSampleRate);
    mLibLoader->builder_setSharingMode(aaudioBuilder,
                                       static_cast<aaudio_sharing_mode_t>(mSharingMode));
    mLibLoader->builder_setPerformanceMode(aaudioBuilder,
                                           static_cast<aaudio_performance_mode_t>(mPerformanceMode));

    // These were added in P so we have to check for the function pointer.
    if (mLibLoader->builder_setUsage != nullptr) {
        mLibLoader->builder_setUsage(aaudioBuilder,
                                     static_cast<aaudio_usage_t>(mUsage));
    }

    if (mLibLoader->builder_setContentType != nullptr) {
        mLibLoader->builder_setContentType(aaudioBuilder,
                                           static_cast<aaudio_content_type_t>(mContentType));
    }

    if (mLibLoader->builder_setInputPreset != nullptr) {
        mLibLoader->builder_setInputPreset(aaudioBuilder,
                                           static_cast<aaudio_input_preset_t>(mInputPreset));
    }

    if (mLibLoader->builder_setSessionId != nullptr) {
        mLibLoader->builder_setSessionId(aaudioBuilder,
                                         static_cast<aaudio_session_id_t>(mSessionId));
    }

    // TODO get more parameters from the builder?

    if (mStreamCallback != nullptr) {
        mLibLoader->builder_setDataCallback(aaudioBuilder, oboe_aaudio_data_callback_proc, this);
        mLibLoader->builder_setFramesPerDataCallback(aaudioBuilder, getFramesPerCallback());
    }
    mLibLoader->builder_setErrorCallback(aaudioBuilder, oboe_aaudio_error_callback_proc, this);

    // ============= OPEN THE STREAM ================
    mAAudioStream = std::make_shared<AAudioStreamOwner>(aaudioBuilder);
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);

    result = static_cast<Result>(mAAudioStream->getOpenResult());
    if (result != Result::OK) {
        goto error2;
    }

    // Hold onto a copy while we finish the open.
    // Probably redundant because we are holding mLock.
    stream = mine->getStream();
    if (stream == nullptr) {
        goto error2;
    }

    // Query and cache the stream properties
    mDeviceId = mLibLoader->stream_getDeviceId(stream);
    mChannelCount = mLibLoader->stream_getChannelCount(stream);
    mSampleRate = mLibLoader->stream_getSampleRate(stream);
    mFormat = static_cast<AudioFormat>(mLibLoader->stream_getFormat(stream));
    mSharingMode = static_cast<SharingMode>(mLibLoader->stream_getSharingMode(stream));
    mPerformanceMode = static_cast<PerformanceMode>(
            mLibLoader->stream_getPerformanceMode(stream));
    mBufferCapacityInFrames = mLibLoader->stream_getBufferCapacity(stream);
    mBufferSizeInFrames = mLibLoader->stream_getBufferSize(stream);

    // These were added in P so we have to check for the function pointer.
    if (mLibLoader->stream_getUsage != nullptr) {
        mUsage = static_cast<Usage>(mLibLoader->stream_getUsage(stream));
    }
    if (mLibLoader->stream_getContentType != nullptr) {
        mContentType = static_cast<ContentType>(mLibLoader->stream_getContentType(stream));
    }
    if (mLibLoader->stream_getInputPreset != nullptr) {
        mInputPreset = static_cast<InputPreset>(mLibLoader->stream_getInputPreset(stream));
    }
    if (mLibLoader->stream_getSessionId != nullptr) {
        mSessionId = static_cast<SessionId>(mLibLoader->stream_getSessionId(stream));
    } else {
        mSessionId = SessionId::None;
    }

    LOGD("AudioStreamAAudio.open() app    format = %d", static_cast<int>(mFormat));
    LOGD("AudioStreamAAudio.open() sample rate   = %d", static_cast<int>(mSampleRate));
    LOGD("AudioStreamAAudio.open() capacity      = %d", static_cast<int>(mBufferCapacityInFrames));

error2:
    mLibLoader->builder_delete(aaudioBuilder);
    LOGD("AudioStreamAAudio.open: AAudioStream_Open() returned %s, stream = %p",
         mLibLoader->convertResultToText(static_cast<aaudio_result_t>(result)),
         stream);
    return result;
}

Result AudioStreamAAudio::close() {
    // The main reason we have this mutex if to prevent a collision between a call
    // by the application to stop a stream at the same time that an onError callback
    // is being executed because of a disconnect. The close will delete the stream,
    // which could otherwise cause the requestStop() to crash.
    std::lock_guard<std::mutex> lock(mLock);

    AudioStream::close();

    // This will delete the AAudio stream object so we need to null out the pointer.
    mAAudioStream.reset();

    return Result::OK;
}

DataCallbackResult AudioStreamAAudio::callOnAudioReady(AAudioStream *stream,
                                                                 void *audioData,
                                                                 int32_t numFrames) {
    DataCallbackResult result = fireDataCallback(audioData, numFrames);
    if (result == DataCallbackResult::Continue) {
        return result;
    } else {
        if (result == DataCallbackResult::Stop) {
            LOGE("Oboe callback returned DataCallbackResult::Stop");
        } else {
            LOGE("Oboe callback returned unexpected value = %d", result);
        }

        if (getSdkVersion() <= __ANDROID_API_P__) {
            launchStopThread();
            if (usesMMap()) {
                return DataCallbackResult::Stop;
            } else {
                // Legacy stream <= API_P cannot be restarted after returning Stop.
                return DataCallbackResult::Continue;
            }
        } else {
            return DataCallbackResult::Stop; // OK >= API_Q
        }
    }
}

void AudioStreamAAudio::onErrorInThread(AAudioStream *stream, Result error) {
    LOGD("onErrorInThread() - entering ===================================");
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

Result AudioStreamAAudio::requestStart() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *aaudioStream = mAAudioStream->getStream();
    if (aaudioStream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(aaudioStream));
            if (state == StreamState::Starting || state == StreamState::Started) {
                // WARNING: On P, AAudio is returning ErrorInvalidState for Output and OK for Input.
                return Result::OK;
            }
        }
        if (mStreamCallback != nullptr) { // Was a callback requested?
            setDataCallbackEnabled(true);
        }
        return static_cast<Result>(mLibLoader->stream_requestStart(aaudioStream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestPause() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *aaudioStream = mAAudioStream->getStream();
    if (aaudioStream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(aaudioStream));
            if (state == StreamState::Pausing || state == StreamState::Paused) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestPause(aaudioStream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestFlush() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *aaudioStream = mAAudioStream->getStream();
    if (aaudioStream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(aaudioStream));
            if (state == StreamState::Flushing || state == StreamState::Flushed) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestFlush(aaudioStream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestStop() {
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *aaudioStream = mAAudioStream->getStream();
    if (aaudioStream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(aaudioStream));
            if (state == StreamState::Stopping || state == StreamState::Stopped) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestStop(aaudioStream));
    } else {
        return Result::ErrorClosed;
    }
}

ResultWithValue<int32_t>   AudioStreamAAudio::write(const void *buffer,
                                     int32_t numFrames,
                                     int64_t timeoutNanoseconds) {
    // Own the stream while in use.
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        int32_t result = mLibLoader->stream_write(aaudioStream, buffer,
                                                  numFrames, timeoutNanoseconds);
        return ResultWithValue<int32_t>::createBasedOnSign(result);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

ResultWithValue<int32_t>   AudioStreamAAudio::read(void *buffer,
                                 int32_t numFrames,
                                 int64_t timeoutNanoseconds) {
    // Own the stream while in use.
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        int32_t result = mLibLoader->stream_read(aaudioStream, buffer,
                                                 numFrames, timeoutNanoseconds);
        return ResultWithValue<int32_t>::createBasedOnSign(result);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

Result AudioStreamAAudio::waitForStateChange(StreamState currentState,
                                        StreamState *nextState,
                                        int64_t timeoutNanoseconds) {
    // Own the stream while in use.
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {

        aaudio_stream_state_t aaudioNextState;
        aaudio_result_t result = mLibLoader->stream_waitForStateChange(
                aaudioStream,
                        static_cast<aaudio_stream_state_t>(currentState),
                        &aaudioNextState,
                        timeoutNanoseconds);
        *nextState = static_cast<StreamState>(aaudioNextState);
        return static_cast<Result>(result);
    } else {
        *nextState = StreamState::Closed;
    }

    return (currentState != *nextState) ? Result::OK : Result::ErrorTimeout;
}

ResultWithValue<int32_t> AudioStreamAAudio::setBufferSizeInFrames(int32_t requestedFrames) {

    // Own the stream while in use.
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();

    if (aaudioStream != nullptr) {

        if (requestedFrames > mBufferCapacityInFrames) {
            requestedFrames = mBufferCapacityInFrames;
        }
        int32_t newBufferSize = mLibLoader->stream_setBufferSize(aaudioStream, requestedFrames);

        // Cache the result if it's valid
        if (newBufferSize > 0) mBufferSizeInFrames = newBufferSize;

        return ResultWithValue<int32_t>::createBasedOnSign(newBufferSize);

    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

StreamState AudioStreamAAudio::getState() {
    // Own the stream while in use.
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        return static_cast<StreamState>(mLibLoader->stream_getState(aaudioStream));
    } else {
        return StreamState::Closed;
    }
}

int32_t AudioStreamAAudio::getBufferSizeInFrames() {
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        mBufferSizeInFrames = mLibLoader->stream_getBufferSize(aaudioStream);
    }
    return mBufferSizeInFrames;
}

int32_t AudioStreamAAudio::getFramesPerBurst() {
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        mFramesPerBurst = mLibLoader->stream_getFramesPerBurst(aaudioStream);
    }
    return mFramesPerBurst;
}

void AudioStreamAAudio::updateFramesRead() {
    AAudioStream *aaudioStream = mAAudioStream->getStream();
    if (aaudioStream != nullptr) {
        mFramesRead = mLibLoader->stream_getFramesRead(aaudioStream);
    }
}

void AudioStreamAAudio::updateFramesWritten() {
    AAudioStream *aaudioStream = mAAudioStream->getStream();
    if (aaudioStream != nullptr) {
        mFramesWritten = mLibLoader->stream_getFramesWritten(aaudioStream);
    }
}

ResultWithValue<int32_t> AudioStreamAAudio::getXRunCount() const {
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        return ResultWithValue<int32_t>::createBasedOnSign(
                mLibLoader->stream_getXRunCount(aaudioStream));
    } else {
        return ResultWithValue<int32_t>(Result::ErrorNull);
    }
}

Result AudioStreamAAudio::getTimestamp(clockid_t clockId,
                                   int64_t *framePosition,
                                   int64_t *timeNanoseconds) {
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        if (getState() != StreamState::Started) {
            return Result::ErrorInvalidState;
        }
        return static_cast<Result>(mLibLoader->stream_getTimestamp(aaudioStream, clockId,
                                               framePosition, timeNanoseconds));
    } else {
        return Result::ErrorNull;
    }
}

ResultWithValue<FrameTimestamp> AudioStreamAAudio::getTimestamp(clockid_t clockId) {
    FrameTimestamp frame;
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        aaudio_result_t result = mLibLoader->stream_getTimestamp(aaudioStream, clockId,
                                                                 &frame.position,
                                                                 &frame.timestamp);
        if (result == AAUDIO_OK){
            return ResultWithValue<FrameTimestamp>(frame);
        } else {
            return ResultWithValue<FrameTimestamp>(static_cast<Result>(result));
        }
    } else {
        return ResultWithValue<FrameTimestamp>(Result::ErrorNull);
    }
}

ResultWithValue<double> AudioStreamAAudio::calculateLatencyMillis() {
    // Get the time that a known audio frame was presented.
    int64_t hardwareFrameIndex;
    int64_t hardwareFrameHardwareTime;
    auto result = getTimestamp(CLOCK_MONOTONIC,
                               &hardwareFrameIndex,
                               &hardwareFrameHardwareTime);
    if (result != oboe::Result::OK) {
        return ResultWithValue<double>(static_cast<Result>(result));
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
    double latencyNanos = static_cast<double>(isOutput
                          ? (appFrameHardwareTime - appFrameAppTime) // hardware is later
                          : (appFrameAppTime - appFrameHardwareTime)); // hardware is earlier
    double latencyMillis = latencyNanos / kNanosPerMillisecond;

    return ResultWithValue<double>(latencyMillis);
}

bool AudioStreamAAudio::usesMMap() {
    std::shared_ptr<AAudioStreamOwner> mine(mAAudioStream);
    AAudioStream *aaudioStream = mine->getStream();
    if (aaudioStream != nullptr) {
        return mLibLoader->stream_isMMapUsed(aaudioStream);
    } else {
        return false;
    }
}

} // namespace oboe
