/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "OboeDebug.h"
#include "DataConversionFlowGraph.h"
#include "SourceFloatCaller.h"

#include <flowgraph/ClipToRange.h>
#include <flowgraph/MonoToMultiConverter.h>
#include <flowgraph/RampLinear.h>
#include <flowgraph/SinkFloat.h>
#include <flowgraph/SinkI16.h>
#include <flowgraph/SinkI24.h>
#include <flowgraph/SourceFloat.h>
#include <flowgraph/SourceI16.h>
#include <flowgraph/SourceI24.h>

using namespace oboe;
using namespace flowgraph;

void DataConversionFlowGraph::setSource(const void *buffer, int32_t numFrames) {
    mSource->setData(buffer, numFrames);
}

// Chain together multiple processors.
Result DataConversionFlowGraph::configure(AudioStream *stream,
                                AudioFormat sourceFormat,
                                int32_t sourceChannelCount,
                                int32_t sourceSampleRate,
                                AudioFormat sinkFormat,
                                int32_t sinkChannelCount,
                                int32_t sinkSampleRate
                                ) {
    AudioFloatOutputPort *lastOutput = nullptr;

    LOGD("%s() flow convert format = %d to %d, channel = %d to %d, rate = %d to %d",
            __func__, sourceFormat, sinkFormat,
            sourceChannelCount, sinkChannelCount,
            sourceSampleRate, sinkSampleRate);

    // Source
    if (stream->getCallback() != nullptr) {
        switch (sourceFormat) {
            case AudioFormat::Float:
                mSourceCaller = std::make_unique<SourceFloatCaller>(sourceChannelCount);
                break;
            case AudioFormat::I16:
                // TODO mSource = std::make_unique<SourceI16>(sourceChannelCount);
                break;
            default:
                LOGE("%s() Unsupported source caller format = %d", __func__, sourceFormat);
                return Result::ErrorIllegalArgument;
        }
        mSourceCaller->setStream(stream);
        mSourceCaller->setCallback(stream->getCallback());
        lastOutput = &mSourceCaller->output;
    } else {
        switch (sourceFormat) {
            case AudioFormat::Float:
                mSource = std::make_unique<SourceFloat>(sourceChannelCount);
                break;
            case AudioFormat::I16:
                mSource = std::make_unique<SourceI16>(sourceChannelCount);
                break;
            default:
                LOGE("%s() Unsupported source format = %d", __func__, sourceFormat);
                return Result::ErrorIllegalArgument;
        }
        lastOutput = &mSource->output;
    }

    // Sample Rate conversion
    if (sourceSampleRate != sinkSampleRate) {
        mRateConverter = std::make_unique<SampleRateConverter>(sourceChannelCount);
        mRateConverter->setPhaseIncrement(((double)sourceSampleRate / sinkSampleRate));
        lastOutput->connect(&mRateConverter->input);
        lastOutput = &mRateConverter->output;
    }

    // Expand the number of channels if required.
    if (sourceChannelCount == 1 && sinkChannelCount > 1) {
        mChannelConverter = std::make_unique<MonoToMultiConverter>(sinkChannelCount);
        lastOutput->connect(&mChannelConverter->input);
        lastOutput = &mChannelConverter->output;
    } else if (sourceChannelCount != sinkChannelCount) {
        LOGE("%s() Channel reduction not supported.", __func__);
        return Result::ErrorUnimplemented;
    }

    // Sink
    switch (sinkFormat) {
        case AudioFormat::Float:
            mSink = std::make_unique<SinkFloat>(sinkChannelCount);
            break;
        case AudioFormat::I16:
            mSink = std::make_unique<SinkI16>(sinkChannelCount);
            break;
        default:
            LOGE("%s() Unsupported sink format = %d", __func__, sinkFormat);
            return Result::ErrorIllegalArgument;;
    }
    lastOutput->connect(&mSink->input);

    mFramePosition = 0;

    return Result::OK;
}

int32_t DataConversionFlowGraph::read(void *buffer, int32_t numFrames) {
    // TODO This only works for OUTPUT. Fix for INPUT.
    int32_t numRead = mSink->read(mFramePosition, buffer, numFrames);
    mFramePosition += numRead;
    return numRead;
}
