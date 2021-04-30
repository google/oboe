/*
 * Copyright 2021 The Android Open Source Project
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

#ifndef OBOETESTER_FORMATCONVERTERBOX_H
#define OBOETESTER_FORMATCONVERTERBOX_H


#include <unistd.h>
#include <sys/types.h>

#include "oboe/Oboe.h"
#include "flowgraph/SinkFloat.h"
#include "flowgraph/SinkI16.h"
#include "flowgraph/SinkI24.h"
#include "flowgraph/SinkI32.h"
#include "flowgraph/SourceFloat.h"
#include "flowgraph/SourceI16.h"
#include "flowgraph/SourceI24.h"
#include "flowgraph/SourceI32.h"

/**
 * Use flowgraph modules to convert between the various data formats.
 */

class FormatConverterBox {
public:
    FormatConverterBox(int32_t numSamples, oboe::AudioFormat inputFormat,
                       oboe::AudioFormat outputFormat);

    void *getOutputBuffer() {
        return (void *) mOutputBuffer.get();
    };

    void *getInputBuffer() {
        return (void *) mInputBuffer.get();
    };

    /** Convert the data in the conversion buffer to float.
     */
    int32_t convert(int32_t numSamples);
    int32_t convert(int32_t numSamples, const void *inputBuffer);
    int32_t convert(void *outputBuffer, int32_t numSamples);
    int32_t convert(void *outputBuffer, int32_t numSamples, const void *inputBuffer);

private:
    oboe::AudioFormat mInputFormat{oboe::AudioFormat::Invalid};
    oboe::AudioFormat mOutputFormat{oboe::AudioFormat::Invalid};

    std::unique_ptr<uint8_t[]> mInputBuffer;
    std::unique_ptr<uint8_t[]> mOutputBuffer;

    std::unique_ptr<oboe::flowgraph::FlowGraphSourceBuffered>  mSource;
    std::unique_ptr<oboe::flowgraph::FlowGraphSink>  mSink;
};


#endif //OBOETESTER_FORMATCONVERTERBOX_H
