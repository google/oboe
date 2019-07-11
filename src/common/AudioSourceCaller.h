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

#ifndef OBOE_AUDIO_SOURCE_CALLER_H
#define OBOE_AUDIO_SOURCE_CALLER_H

#include "OboeDebug.h"
#include "oboe/Oboe.h"

#include "flowgraph/AudioProcessorBase.h"
#include "FixedBlockReader.h"

namespace oboe {

class AudioStreamCallback;
class AudioStream;

class AudioSourceCaller : public flowgraph::AudioSource, public FixedBlockProcessor {
public:
    AudioSourceCaller(int32_t channelCount, int32_t framesPerCallback, int32_t bytesPerSample)
            : AudioSource(channelCount)
            , mBlockReader(*this) {
        mBlockReader.open(channelCount * framesPerCallback * bytesPerSample);
    }

    void setCallback(oboe::AudioStreamCallback *streamCallback) {
        mStreamCallback = streamCallback;
    }

    void setStream(oboe::AudioStream *stream) {
        mStream = stream;
    }

    int32_t onProcessFixedBlock(uint8_t *buffer, int32_t numBytes) override;

protected:
    oboe::AudioStreamCallback *mStreamCallback = nullptr;
    oboe::AudioStream         *mStream = nullptr;

    FixedBlockReader           mBlockReader;
    DataCallbackResult         mCallbackResult = DataCallbackResult::Continue;
};

}
#endif //OBOE_AUDIO_SOURCE_CALLER_H
