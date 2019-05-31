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


#ifndef OBOE_OBOE_FLOW_GRAPH_H
#define OBOE_OBOE_FLOW_GRAPH_H

#include <memory>
#include <stdint.h>
#include <sys/types.h>

#include <flowgraph/MonoToMultiConverter.h>
#include <flowgraph/SampleRateConverter.h>
#include <oboe/Definitions.h>
#include "AudioSourceCaller.h"

namespace oboe {

class AudioStream;
class AudioSourceCaller;

class DataConversionFlowGraph {
public:

    void setSource(const void *buffer, int32_t numFrames);

    /** Connect several modules together to convert from source to sink.
     * This should only be called once for each instance.
     *
     * @param sourceFormat
     * @param sourceChannelCount
     * @param sinkFormat
     * @param sinkChannelCount
     * @return
     */
    oboe::Result configure(oboe::AudioStream *stream,
                           oboe::AudioFormat sourceFormat,
                           int32_t sourceChannelCount,
                           int32_t sourceSampleRate,
                           oboe::AudioFormat sinkFormat,
                           int32_t sinkChannelCount,
                           int32_t sinkSampleRate);

    int32_t read(void *buffer, int32_t numFrames);


private:
    std::unique_ptr<flowgraph::AudioSource>          mSource;
    std::unique_ptr<AudioSourceCaller>               mSourceCaller;
    std::unique_ptr<flowgraph::MonoToMultiConverter> mChannelConverter;
    std::unique_ptr<flowgraph::SampleRateConverter>  mRateConverter;
    std::unique_ptr<flowgraph::AudioSink>            mSink;

    int64_t mFramePosition = 0;
};

}
#endif //OBOE_OBOE_FLOW_GRAPH_H
