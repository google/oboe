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

#include <trace.h>
#include <inttypes.h>
#include <memory>

#include "shared/Oscillator.h"

#include "PlayAudioEngine.h"
#include "SoundGenerator.h"



PlayAudioEngine::PlayAudioEngine() : TapAudioEngine() {
    // Initialize the trace functions, this enables you to output trace statements without
    // blocking. See https://developer.android.com/studio/profile/systrace-commandline.html
    Trace::initialize();
}

/**
 * Creates an audio stream for playback. Takes in a builder pointer which contains stream params
 */
void PlayAudioEngine::createPlaybackStream(oboe::AudioStreamBuilder &builder) {

        TapAudioEngine::createPlaybackStream(builder);
        mIsLatencyDetectionSupported = (mStream->getTimestamp(CLOCK_MONOTONIC) !=
                                        oboe::Result::ErrorUnimplemented);
        getCallbackPtr()->setLatencyDetectionEnabled(mIsLatencyDetectionSupported);
        getCallbackPtr()->setOutputLatencyMillis(&mCurrentOutputLatencyMillis);
        mLatencyTuner = std::make_unique<oboe::LatencyTuner>(*mStream);
        getCallbackPtr()->setLatencyTuner(mLatencyTuner.get());
}

double PlayAudioEngine::getCurrentOutputLatencyMillis() {
    return mCurrentOutputLatencyMillis;
}

void PlayAudioEngine::setBufferSizeInBursts(int32_t numBursts) {
    auto ptr = static_cast<CustomAudioStreamCallback*>(mCallback.get());
    ptr->setBufferTuneEnabled(numBursts == kBufferSizeAutomatic);
    auto result = mStream->setBufferSizeInFrames(
            numBursts * mStream->getFramesPerBurst());
    if (result) {
        LOGD("Buffer size successfully changed to %d", result.value());
    } else {
        LOGW("Buffer size could not be changed, %d", result.error());
    }
}

bool PlayAudioEngine::isLatencyDetectionSupported() {
    return mIsLatencyDetectionSupported;
}
void PlayAudioEngine::setAudioApi(oboe::AudioApi audioApi) {
    oboe::AudioStreamBuilder *builder = oboe::AudioStreamBuilder(*mStream).setAudioApi(audioApi);
    createPlaybackStream(*builder);
    LOGD("AudioAPI is now %d", mStream->getAudioApi());
}

void PlayAudioEngine::setChannelCount(int channelCount) {
    oboe::AudioStreamBuilder *builder = oboe::AudioStreamBuilder(*mStream).setChannelCount(channelCount);
    createPlaybackStream(*builder);
    LOGD("Channel count is now %d", mStream->getChannelCount());
}

/**
 * Set the audio device which should be used for playback. Can be set to oboe::kUnspecified if
 * you want to use the default playback device (which is usually the built-in speaker if
 * no other audio devices, such as headphones, are attached).
 *
 * @param deviceId the audio device id, can be obtained through an {@link AudioDeviceInfo} object
 * using Java/JNI.
 */
void PlayAudioEngine::setDeviceId(int32_t deviceId) {
    oboe::AudioStreamBuilder *builder = oboe::AudioStreamBuilder(*mStream).setDeviceId(deviceId);
    createPlaybackStream(*builder);
    LOGD("Device ID is now %d", mStream->getDeviceId());
}

