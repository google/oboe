/*
 * Copyright 2022 The Android Open Source Project
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

#include <stdlib.h>
#include <aaudio/AAudioExtensions.h>

#include "common/OboeDebug.h"
#include "common/AudioClock.h"
#include "TestRoutingCrash.h"

using namespace oboe;

int32_t TestRoutingCrash::start() {

    mDataCallback = std::make_shared<MyDataCallback>(this);

    // Disable MMAP because we are trying to crash a Legacy Stream.
    bool wasMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    AAudioExtensions::getInstance().setMMapEnabled(false);

    AudioStreamBuilder builder;
    oboe::Result result = builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(kChannelCount)
            ->setBufferCapacityInFrames(32000)
            ->setDataCallback(mDataCallback)
            ->setUsage(oboe::Usage::VoiceCommunication) // so we can reroute it
            ->openStream(mStream);
    if (result != oboe::Result::OK) {
        return (int32_t) result;
    }

    mStream->setBufferSizeInFrames(mStream->getBufferCapacityInFrames());

    AAudioExtensions::getInstance().setMMapEnabled(wasMMapEnabled);
    return (int32_t) mStream->requestStart();
}

int32_t TestRoutingCrash::stop() {
    oboe::Result result1 =  mStream->requestStop();
    oboe::Result result2 =   mStream->close();
    return (int32_t)((result1 != oboe::Result::OK) ? result1 : result2);
}

DataCallbackResult TestRoutingCrash::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    float *output = (float *) audioData;

    // Sleep so that we spend most of our time in the callback without underflowing.
    // Otherwise the window for the crash is very narrow.
    int64_t now = AudioClock::getNanoseconds();
    int64_t elapsed = now - mLastCallbackTime;
    double bufferTimeNanos = 1.0e9 * numFrames / (double) audioStream->getSampleRate();
    int64_t targetDuration = (int64_t) (bufferTimeNanos * 0.7);
    mParent->sleepTimeNanos = targetDuration - elapsed;
    AudioClock::sleepForNanos(mParent->sleepTimeNanos);

    // Try to trigger a restoreTrack_l() in AudioFlinger.
    audioStream->getTimestamp(CLOCK_MONOTONIC);

    // Fill mono buffer with a sine wave.
    // If the routing occurred then the buffer may be dead and
    // we may be writing into unallocated memory.
    int numSamples = numFrames * kChannelCount;
    for (int i = 0; i < numSamples; i++) {
        *output++ = sinf(mPhase) * 0.2f;
        mPhase += mPhaseIncrement;
        // Wrap the phase around the circular.
        if (mPhase >= M_PI) mPhase -= 2 * M_PI;
    }

    mLastCallbackTime = AudioClock::getNanoseconds();

    return oboe::DataCallbackResult::Continue;
}
