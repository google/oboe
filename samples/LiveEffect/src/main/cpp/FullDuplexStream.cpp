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

#include "FullDuplexStream.h"

oboe::DataCallbackResult FullDuplexStream::onAudioReady(
        oboe::AudioStream *outputStream,
        void *audioData,
        int numFrames) {
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;
    int32_t actualFramesRead = 0;

    // Silence the output.
    int32_t numBytes = numFrames * outputStream->getBytesPerFrame();
    memset(audioData, 0 /* value */, numBytes);

    if (mCountCallbacksToDrain > 0) {
        // Drain the input.
        int32_t totalFramesRead = 0;
        do {
            oboe::ResultWithValue<int32_t> result = getInputStream()->read(mInputBuffer.get(),
                                                                           numFrames,
                                                                           0 /* timeout */);
            if (!result) {
                // Ignore errors because input stream may not be started yet.
                break;
            }
            actualFramesRead = result.value();
            totalFramesRead += actualFramesRead;
        } while (actualFramesRead > 0);
        // Only counts if we actually got some data.
        if (totalFramesRead > 0) {
            mCountCallbacksToDrain--;
        }

    } else if (mCountInputBurstsCushion > 0) {
        // Let the input fill up a bit so we are not so close to the write pointer.
        mCountInputBurstsCushion--;

    } else if (mCountCallbacksToDiscard > 0) {
        // Ignore. Allow the input to reach to equilibrium with the output.
        oboe::ResultWithValue<int32_t> result = getInputStream()->read(mInputBuffer.get(),
                                                                       numFrames,
                                                                       0 /* timeout */);
        if (!result) {
            callbackResult = oboe::DataCallbackResult::Stop;
        }
        mCountCallbacksToDiscard--;

    } else {
        // Read data into input buffer.
        oboe::ResultWithValue<int32_t> result = getInputStream()->read(mInputBuffer.get(),
                                                                       numFrames,
                                                                       0 /* timeout */);
        if (!result) {
            callbackResult = oboe::DataCallbackResult::Stop;
        } else {
            int32_t framesRead = result.value();

            callbackResult = onBothStreamsReady(
                    mInputBuffer.get(), framesRead,
                    audioData, numFrames
            );
        }
    }

    if (callbackResult == oboe::DataCallbackResult::Stop) {
        getInputStream()->requestStop();
    }

    return callbackResult;
}

oboe::Result FullDuplexStream::start() {
    mCountCallbacksToDrain = kNumCallbacksToDrain;
    mCountInputBurstsCushion = mNumInputBurstsCushion;
    mCountCallbacksToDiscard = kNumCallbacksToDiscard;

    // Determine maximum size that could possibly be called.
    int32_t bufferSize = getOutputStream()->getBufferCapacityInFrames()
            * getOutputStream()->getChannelCount();
    if (bufferSize > mBufferSize) {
        mInputBuffer = std::make_unique<float[]>(bufferSize);
        mBufferSize = bufferSize;
    }
    oboe::Result result = getInputStream()->requestStart();
    if (result != oboe::Result::OK) {
        return result;
    }
    return getOutputStream()->requestStart();
}

oboe::Result FullDuplexStream::stop() {
    oboe::Result outputResult = oboe::Result::OK;
    oboe::Result inputResult = oboe::Result::OK;
    if (getOutputStream()) {
        outputResult = getOutputStream()->requestStop();
    }
    if (getInputStream()) {
        inputResult = getInputStream()->requestStop();
    }
    if (outputResult != oboe::Result::OK) {
        return outputResult;
    } else {
        return inputResult;
    }
}

int32_t FullDuplexStream::getNumInputBurstsCushion() const {
    return mNumInputBurstsCushion;
}

void FullDuplexStream::setNumInputBurstsCushion(int32_t numBursts) {
    FullDuplexStream::mNumInputBurstsCushion = numBursts;
}
