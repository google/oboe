/*
 * Copyright 2019 The Android Open Source Project
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

#include "common/OboeDebug.h"
#include "FullDuplexEcho.h"

oboe::Result  FullDuplexEcho::start() {
    mDelaySizeFrames = 3 * getOutputStream()->getSampleRate();
    static const int32_t maxDelaySamples = mDelaySizeFrames * getOutputStream()->getChannelCount();
    mDelayLine = std::make_unique<float[]>(maxDelaySamples);
    return FullDuplexStream::start();
}

oboe::DataCallbackResult FullDuplexEcho::onBothStreamsReady(
        const void *inputData,
        int   numInputFrames,
        void *outputData,
        int   numOutputFrames) {
    // FIXME only handles matching stream formats.
//    LOGE("FullDuplexEcho::%s() called, numInputFrames = %d, numOutputFrames = %d",
//            __func__, numInputFrames, numOutputFrames);
    int32_t framesLeft = std::min(numInputFrames, numOutputFrames);
    while (framesLeft > 0) {
        float *delayAddress = mDelayLine.get() + (mCursorFrames * getOutputStream()->getChannelCount());
        memcpy(outputData, delayAddress, getOutputStream()->getBytesPerFrame());
        memcpy(delayAddress, inputData, getOutputStream()->getBytesPerFrame());
        mCursorFrames++;
        if (mCursorFrames >= mDelaySizeFrames) {
            mCursorFrames = 0;
        }
        inputData = ((uint8_t *)inputData) + getOutputStream()->getBytesPerFrame();
        outputData = ((uint8_t *)outputData) + getOutputStream()->getBytesPerFrame();
        framesLeft--;
    }
    return oboe::DataCallbackResult::Continue;
};
