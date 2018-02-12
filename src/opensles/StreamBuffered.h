/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OBOE_STREAM_BUFFERED_H
#define OBOE_STREAM_BUFFERED_H

#include "common/OboeDebug.h"
#include "oboe/Stream.h"
#include "oboe/StreamCallback.h"
#include "fifo/FifoBuffer.h"

namespace oboe {

// A stream that contains a FIFO buffer.
class StreamBuffered : public Stream {
public:

    StreamBuffered();
    explicit StreamBuffered(const StreamBuilder &builder);

    Result open() override;

    int32_t write(const void *buffer,
                  int32_t numFrames,
                  int64_t timeoutNanoseconds) override;

    Result setBufferSizeInFrames(int32_t requestedFrames) override;

    int32_t getBufferSizeInFrames() const override;

    int32_t getBufferCapacityInFrames() const override;

protected:

    class AudioStreamBufferedCallback : public StreamCallback {
    public:
        AudioStreamBufferedCallback(StreamBuffered *bufferedStream)
                : mBufferedStream(bufferedStream) {
        }

        virtual ~AudioStreamBufferedCallback() {}

        virtual DataCallbackResult onAudioReady(
                Stream *audioStream,
                void *audioData,
                int numFrames) {
            int32_t framesRead = mBufferedStream->mFifoBuffer->readNow(audioData, numFrames);
            //LOGD("AudioStreamBufferedCallback(): read %d / %d frames", framesRead, numFrames);
            return (framesRead >= 0) ? DataCallbackResult::Continue : DataCallbackResult::Stop;
        }

        virtual void onExit(Result reason) {}
    private:
        StreamBuffered *mBufferedStream;
    };

private:

    FifoBuffer *mFifoBuffer;
};

} // namespace oboe

#endif //OBOE_STREAM_BUFFERED_H
