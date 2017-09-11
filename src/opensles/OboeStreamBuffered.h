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

#ifndef OBOE_OBOE_STREAM_BUFFERED_H
#define OBOE_OBOE_STREAM_BUFFERED_H

#include "common/OboeDebug.h"
#include "oboe/OboeStream.h"
#include "oboe/OboeStreamCallback.h"
#include "fifo/FifoBuffer.h"

// A stream that contains a FIFO buffer.
class OboeStreamBuffered : public OboeStream {
public:

    OboeStreamBuffered();
    explicit OboeStreamBuffered(const OboeStreamBuilder &builder);
    virtual ~OboeStreamBuffered();

    oboe_result_t open() override;

    oboe_result_t write(const void *buffer,
                                int32_t numFrames,
                                int64_t timeoutNanoseconds) override;

    oboe_result_t setBufferSizeInFrames(int32_t requestedFrames) override;

    int32_t getBufferSizeInFrames() const override;

    int32_t getBufferCapacityInFrames() const override;

protected:

    class AudioStreamBufferedCallback : public OboeStreamCallback {
    public:
        AudioStreamBufferedCallback(OboeStreamBuffered *bufferedStream)
                : mBufferedStream(bufferedStream) {
        }

        virtual ~AudioStreamBufferedCallback() {}

        virtual oboe_result_t onAudioReady(
                OboeStream *audioStream,
                void *audioData,
                int numFrames) {
            int32_t framesRead = mBufferedStream->mFifoBuffer->readNow(audioData, numFrames);
            //LOGD("AudioStreamBufferedCallback(): read %d / %d frames", framesRead, numFrames);
            return (framesRead >= 0) ? OBOE_OK : OBOE_ERROR_INTERNAL;
        }

        virtual void onExit(oboe_result_t reason) {}
    private:
        OboeStreamBuffered *mBufferedStream;
    };

private:

    FifoBuffer *mFifoBuffer;
    AudioStreamBufferedCallback *mInternalCallback;
};


#endif //OBOE_OBOE_STREAM_BUFFERED_H
