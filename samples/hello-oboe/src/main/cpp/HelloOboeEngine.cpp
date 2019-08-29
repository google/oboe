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
HelloOboeEngine::HelloOboeEngine(): mLatencyCallback(std::make_unique<LatencyTuningCallback>(*this)) {
    start();
    updateLatencyDetection();
}

double HelloOboeEngine::getCurrentOutputLatencyMillis() {
    if (!mIsLatencyDetectionSupported) return -1;
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
    mIsLatencyDetectionSupported = false;
    mLatencyCallback->setBufferTuneEnabled(numBursts == kBufferSizeAutomatic);
    auto result = mStream->setBufferSizeInFrames(
            numBursts * mStream->getFramesPerBurst());
    updateLatencyDetection();
    if (result) {
        LOGD("Buffer size successfully changed to %d", result.value());
    } else {
        LOGW("Buffer size could not be changed, %d", result.error());
    }
}

void HelloOboeEngine::setAudioApi(oboe::AudioApi audioApi) {
    mIsLatencyDetectionSupported = false;
    createPlaybackStream(*oboe::AudioStreamBuilder(*mStream)
            .setAudioApi(audioApi));
    updateAudioSource();
    LOGD("AudioAPI is now %d", mStream->getAudioApi());
}

void HelloOboeEngine::setChannelCount(int channelCount) {
    mIsLatencyDetectionSupported = false;
    createPlaybackStream(*oboe::AudioStreamBuilder(*mStream)
            .setChannelCount(channelCount));
    updateAudioSource();
    LOGD("Channel count is now %d", mStream->getChannelCount());
}

void HelloOboeEngine::setDeviceId(int32_t deviceId) {
    mIsLatencyDetectionSupported = false;
    createPlaybackStream(*oboe::AudioStreamBuilder(*mStream).
            setDeviceId(deviceId));
    updateAudioSource();
    LOGD("Device ID is now %d", mStream->getDeviceId());
}

bool HelloOboeEngine::isLatencyDetectionSupported() {
    return mIsLatencyDetectionSupported;
}

void HelloOboeEngine::updateLatencyDetection() {
    mIsLatencyDetectionSupported = (mStream->getTimestamp((CLOCK_MONOTONIC)) !=
                                    oboe::Result::ErrorUnimplemented);
}

void HelloOboeEngine::tap(bool isDown) {
    mAudioSource->tap(isDown);
}

void HelloOboeEngine::updateAudioSource() {
    *mAudioSource = SoundGenerator(mStream->getSampleRate(), mStream->getChannelCount());
    mStream->start();
    updateLatencyDetection();
}

oboe::Result HelloOboeEngine::createPlaybackStream(oboe::AudioStreamBuilder builder) {
    return builder.setSharingMode(oboe::SharingMode::Exclusive)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setFormat(oboe::AudioFormat::Float)
        ->setCallback(mLatencyCallback.get())
        ->openManagedStream(mStream);
}

void HelloOboeEngine::restart() {
    start();
}

void HelloOboeEngine::start() {
    auto result = createPlaybackStream(oboe::AudioStreamBuilder());
    if (result == oboe::Result::OK){
        mAudioSource =  std::make_shared<SoundGenerator>(mStream->getSampleRate(), mStream->getChannelCount());
        mLatencyCallback->setSource(std::dynamic_pointer_cast<IRenderableAudio>(mAudioSource));
        mStream->start();
    } else {
        LOGE("Error creating playback stream. Error: %s", oboe::convertToText(result));
    }
}

