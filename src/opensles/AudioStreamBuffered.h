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

#include <cstring>
#include <assert.h>
#include "common/OboeDebug.h"
#include "oboe/AudioStream.h"
#include "oboe/AudioStreamCallback.h"
#include "fifo/FifoBuffer.h"

namespace oboe {

// A stream that contains a FIFO buffer.
// This is used to implement blocking reads and writes.
class AudioStreamBuffered : public AudioStream {
public:

    AudioStreamBuffered();
    explicit AudioStreamBuffered(const AudioStreamBuilder &builder);
    ~AudioStreamBuffered();

    Result finishOpen();

    int32_t write(const void *buffer,
                  int32_t numFrames,
                  int64_t timeoutNanoseconds) override;

    int32_t read(void *buffer,
                 int32_t numFrames,
                 int64_t timeoutNanoseconds) override;

    Result setBufferSizeInFrames(int32_t requestedFrames) override;

    int32_t getBufferSizeInFrames() const override;

    int32_t getBufferCapacityInFrames() const override;

protected:

    class AudioStreamBufferedCallback : public AudioStreamCallback {
    public:
        AudioStreamBufferedCallback(AudioStreamBuffered *bufferedStream)
                : mBufferedStream(bufferedStream) {
        }

        virtual ~AudioStreamBufferedCallback() {}

        virtual DataCallbackResult onAudioReady(
                AudioStream *audioStream,
                void *audioData,
                int numFrames) {
            int32_t framesTransferred  = 0;
            static int transferCount = 0;

            if (mBufferedStream->getDirection() == oboe::Direction::Output) {
                // This OUTPUT callback will read from the FIFO and write to audioData
                framesTransferred = mBufferedStream->mFifoBuffer->readNow(audioData, numFrames);
                LOGV("AudioStreamBufferedCallback::onAudioReady() read %d / %d frames from FIFO, #%d",
                     framesTransferred, numFrames, transferCount);
            } else {
                // This INPUT callback will read from audioData and write to the FIFO
                framesTransferred = mBufferedStream->mFifoBuffer->write(audioData, numFrames);
                LOGD("AudioStreamBufferedCallback::onAudioReady() wrote %d / %d frames to FIFO, #%d",
                     framesTransferred, numFrames, transferCount);
            }
            transferCount++;
            // return (framesTransferred >= 0) ? DataCallbackResult::Continue : DataCallbackResult::Stop;
            return DataCallbackResult::Continue;
        }

        virtual void onExit(Result reason) {}
    private:
        AudioStreamBuffered *mBufferedStream;
    };

private:

    FifoBuffer                                  *mFifoBuffer = nullptr;
    std::unique_ptr<AudioStreamBufferedCallback> mInternalCallback;
};

} // namespace oboe

#endif //OBOE_STREAM_BUFFERED_H
