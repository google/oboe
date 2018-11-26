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

#include <cstring>
#include <sched.h>

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"
#include "AudioStreamGateway.h"

AudioStreamGateway::AudioStreamGateway(int samplesPerFrame)
        : input(*this, samplesPerFrame)
        , mFramePosition(0)
{
}

AudioStreamGateway::~AudioStreamGateway()
{
}

oboe::DataCallbackResult AudioStreamGateway::onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int numFrames) {
    int framesLeft = numFrames;
    int16_t *shortData = (int16_t *) audioData;
    float *floatData = (float *) audioData;
    AudioResult result = 0;

#ifdef OBOE_TESTER_DEBUG_STOP
    if (mFrameCountdown <= 0) {
        LOGI("%s() : mCallCounter = %d\n", __func__, mCallCounter);
        mFrameCountdown = 4800;
    }
    mFrameCountdown -= numFrames;
    mCallCounter++;
    if (mCallCounter > 500) {
        LOGI("%s() : return STOP\n", __func__);
        return oboe::DataCallbackResult::Stop;
    }
#endif /* OBOE_TESTER_DEBUG_STOP */

    if (!mSchedulerChecked) {
        mScheduler = sched_getscheduler(gettid());
        mSchedulerChecked = true;
    }

    while (framesLeft > 0) {
        // Do not process more than the MAX block size in one pass.
        int framesToPlay = framesLeft;
        if (framesToPlay > MAX_BLOCK_SIZE) {
            framesToPlay = MAX_BLOCK_SIZE;
        }
        // Run the graph and pull data through the input port.
        result = onProcess(mFramePosition, framesToPlay);
        if (result < 0) {
            break;
        }
        const float *signal = input.getFloatBuffer(framesToPlay);
        int32_t numSamples = framesToPlay * input.getSamplesPerFrame();
        if (audioStream->getFormat() == oboe::AudioFormat::I16) {
            oboe::convertFloatToPcm16(signal, shortData, numSamples);
            shortData += numSamples;
        } else if (audioStream->getFormat() == oboe::AudioFormat::Float) {
            memcpy(floatData, signal, numSamples * sizeof(float));
            floatData += numSamples;
        }
        mFramePosition += framesToPlay;
        framesLeft -= framesToPlay;
    }
    return (result < 0) ? oboe::DataCallbackResult::Stop : oboe::DataCallbackResult::Continue;
}

AudioResult AudioStreamGateway::onProcess(
        uint64_t framePosition,
        int numFrames) {
    AudioResult result = input.pullData(framePosition, numFrames);
    return result;
}

int AudioStreamGateway::getScheduler() {
    return mScheduler;
}
