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

#include <memory>

#include "FilterAudioStream.h"

using namespace oboe;
using namespace flowgraph;

Result FilterAudioStream::configureFlowGraph() {
    mFlowGraph = std::make_unique<DataConversionFlowGraph>();
    bool isOutput = getDirection() == Direction::Output;

    const oboe::AudioFormat sourceFormat = isOutput ? getFormat() : mChildStream->getFormat();
    const int32_t sourceChannelCount = isOutput ? getChannelCount() : mChildStream->getChannelCount();
    const int32_t sourceSampleRate = isOutput ? getSampleRate() : mChildStream->getSampleRate();

    const oboe::AudioFormat sinkFormat = isOutput ? mChildStream->getFormat() : getFormat();
    const int32_t sinkChannelCount = isOutput ? mChildStream->getChannelCount() : getChannelCount();
    const int32_t sinkSampleRate = isOutput ? mChildStream->getSampleRate() : getSampleRate();

    mRateScaler = ((double) sourceSampleRate) / sinkSampleRate;

    return mFlowGraph->configure(this,
                          sourceFormat,
                          sourceChannelCount,
                          sourceSampleRate,
                          sinkFormat,
                          sinkChannelCount,
                          sinkSampleRate
    );
}


ResultWithValue<int32_t> FilterAudioStream::write(const void *buffer,
                               int32_t numFrames,
                               int64_t timeoutNanoseconds) {
    mFlowGraph->setSource(buffer, numFrames);
    while (true) {
        int32_t numRead = mFlowGraph->read(mBlockingBuffer.get(), getFramesPerBurst());
        if (numRead < 0) {
            return ResultWithValue<int32_t>::createBasedOnSign(numFrames);
        }
        if (numRead == 0) {
            break;
        }
        auto writeResult = mChildStream->write(mBlockingBuffer.get(),
                                               numRead,
                                               timeoutNanoseconds);
        if (!writeResult) {
            return writeResult;
        }
    }
// This doesn't account for frames left in the flowgraph if there is a timeout.
    return ResultWithValue<int32_t>::createBasedOnSign(numFrames);
}
