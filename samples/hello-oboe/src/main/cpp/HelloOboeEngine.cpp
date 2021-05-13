/**
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


#include <inttypes.h>
#include <memory>

#include <Oscillator.h>

#include "HelloOboeEngine.h"
#include "SoundGenerator.h"


/**
 * Main audio engine for the HelloOboe sample. It is responsible for:
 *
 * - Creating a callback object which is supplied when constructing the audio stream, and will be
 * called when the stream starts
 * - Restarting the stream when user-controllable properties (Audio API, channel count etc) are
 * changed, and when the stream is disconnected (e.g. when headphones are attached)
 * - Calculating the audio latency of the stream
 *
 */
HelloOboeEngine::HelloOboeEngine()
        : mLatencyCallback(std::make_unique<LatencyTuningCallback>()),
        mErrorCallback(std::make_unique<DefaultErrorCallback>(*this)) {
}

double HelloOboeEngine::getCurrentOutputLatencyMillis() {
    if (!mIsLatencyDetectionSupported) return -1.0;

    std::lock_guard<std::mutex> lock(mLock);
    if (!mStream) return -1.0;

    // Get the time that a known audio frame was presented for playing
    auto result = mStream->getTimestamp(CLOCK_MONOTONIC);
    double outputLatencyMillis = -1;
    const int64_t kNanosPerMillisecond = 1000000;
    if (result == oboe::Result::OK) {
        oboe::FrameTimestamp playedFrame = result.value();
        // Get the write index for the next audio frame
        int64_t writeIndex = mStream->getFramesWritten();
        // Calculate the number of frames between our known frame and the write index
        int64_t frameIndexDelta = writeIndex - playedFrame.position;
        // Calculate the time which the next frame will be presented
        int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) /  (mStream->getSampleRate());
        int64_t nextFramePresentationTime = playedFrame.timestamp + frameTimeDelta;
        // Assume that the next frame will be written at the current time
        using namespace std::chrono;
        int64_t nextFrameWriteTime =
                duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
        // Calculate the latency
        outputLatencyMillis = static_cast<double>(nextFramePresentationTime - nextFrameWriteTime)
                         / kNanosPerMillisecond;
    } else {
        LOGE("Error calculating latency: %s", oboe::convertToText(result.error()));
    }
    return outputLatencyMillis;
}

void HelloOboeEngine::setBufferSizeInBursts(int32_t numBursts) {
    std::lock_guard<std::mutex> lock(mLock);
    if (!mStream) return;

    mLatencyCallback->setBufferTuneEnabled(numBursts == kBufferSizeAutomatic);
    auto result = mStream->setBufferSizeInFrames(
            numBursts * mStream->getFramesPerBurst());
    if (result) {
        LOGD("Buffer size successfully changed to %d", result.value());
    } else {
        LOGW("Buffer size could not be changed, %d", result.error());
    }
}

void HelloOboeEngine::setAudioApi(oboe::AudioApi audioApi) {
    if (mAudioApi != audioApi) {
        mAudioApi = audioApi;
        reopenStream();
    }
}

void HelloOboeEngine::setChannelCount(int channelCount) {
    if (mChannelCount != channelCount) {
        mChannelCount = channelCount;
        reopenStream();
    }
}

void HelloOboeEngine::setDeviceId(int32_t deviceId) {
    if (mDeviceId != deviceId) {
        mDeviceId = deviceId;
        if (reopenStream() != oboe::Result::OK) {
            LOGW("Open stream failed, forcing deviceId to Unspecified");
            mDeviceId = oboe::Unspecified;
        }
    }
}

bool HelloOboeEngine::isLatencyDetectionSupported() {
    return mIsLatencyDetectionSupported;
}

void HelloOboeEngine::tap(bool isDown) {
    if (mAudioSource) {
        mAudioSource->tap(isDown);
    }
}

oboe::Result HelloOboeEngine::openPlaybackStream() {
    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setSharingMode(oboe::SharingMode::Exclusive)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setFormat(oboe::AudioFormat::Float)
        ->setDataCallback(mLatencyCallback.get())
        ->setErrorCallback(mErrorCallback.get())
        ->setAudioApi(mAudioApi)
        ->setChannelCount(mChannelCount)
        ->setDeviceId(mDeviceId)
        ->openStream(mStream);
    if (result == oboe::Result::OK) {
        mChannelCount = mStream->getChannelCount();
    }
    return result;
}

void HelloOboeEngine::restart() {
    // The stream will have already been closed by the error callback.
    mLatencyCallback->reset();
    start();
}

oboe::Result HelloOboeEngine::start() {
    std::lock_guard<std::mutex> lock(mLock);
    mIsLatencyDetectionSupported = false;
    auto result = openPlaybackStream();
    if (result == oboe::Result::OK){
        mAudioSource =  std::make_shared<SoundGenerator>(mStream->getSampleRate(),
                mStream->getChannelCount());
        mLatencyCallback->setSource(std::dynamic_pointer_cast<IRenderableAudio>(mAudioSource));

        LOGD("Stream opened: AudioAPI = %d, channelCount = %d, deviceID = %d",
                 mStream->getAudioApi(),
                 mStream->getChannelCount(),
                 mStream->getDeviceId());

        result = mStream->start();
        if (result != oboe::Result::OK) {
            LOGE("Error starting playback stream. Error: %s", oboe::convertToText(result));
            mStream->close();
            mStream.reset();
        } else {
            mIsLatencyDetectionSupported = (mStream->getTimestamp((CLOCK_MONOTONIC)) !=
                                            oboe::Result::ErrorUnimplemented);
        }
    } else {
        LOGE("Error creating playback stream. Error: %s", oboe::convertToText(result));
    }
    return result;
}

oboe::Result HelloOboeEngine::stop() {
    oboe::Result result = oboe::Result::OK;
    // Stop, close and delete in case not already closed.
    std::lock_guard<std::mutex> lock(mLock);
    if (mStream) {
        result = mStream->stop();
        mStream->close();
        mStream.reset();
    }
    return result;
}

oboe::Result HelloOboeEngine::reopenStream() {
    if (mStream) {
        stop();
        return start();
    } else {
        return oboe::Result::OK;
    }
}
