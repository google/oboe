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
#include "FullDuplexStream.h"

oboe::DataCallbackResult FullDuplexStream::onAudioReady(
        oboe::AudioStream *outputStream,
        void *audioData,
        int numFrames) {
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;

//    LOGE("FullDuplexStream::%s() called, numFrames = %d, buf = %p", __func__, numFrames, mInputBuffer.get());
    // Process in multiple bursts to fit in the buffer.
    int32_t framesLeft = numFrames;
    while(framesLeft > 0) {
        // Read data into input buffer.
        int32_t framesToRead = std::min(framesLeft, outputStream->getFramesPerBurst());
        oboe::ResultWithValue<int32_t> result = getInputStream()->read(mInputBuffer.get(),
                framesToRead,
                0 /* timeout */);

        if (!result) {
            LOGE("%s() : read() returned %s\n", __func__, convertToText(result.error()));
            break;
        }
        int32_t framesRead = result.value();

        callbackResult = onBothStreamsReady(
                mInputBuffer.get(), framesRead,
                audioData, framesToRead
                );
        if (callbackResult != oboe::DataCallbackResult::Continue) {
            break;
        }

        framesLeft -= framesToRead;
        audioData = ((uint8_t *)audioData) +
                (framesToRead * outputStream->getChannelCount() * outputStream->getBytesPerSample());
    }

    return callbackResult;
}

oboe::Result FullDuplexStream::start() {
    int32_t bufferSize = getOutputStream()->getFramesPerBurst()
            * getOutputStream()->getChannelCount();
    if (bufferSize > mBufferSize) {
        LOGE("FullDuplexStream::%s() allocating bufferSize = %d", __func__, bufferSize);
        mInputBuffer = std::make_unique<float[]>(bufferSize);
        mBufferSize = bufferSize;
    }
    getInputStream()->requestStart();
    return getOutputStream()->requestStart();
}

oboe::Result FullDuplexStream::stop() {
    getOutputStream()->requestStop(); // TODO result?
    return getInputStream()->requestStop();
}
