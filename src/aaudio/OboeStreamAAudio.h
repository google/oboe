/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef OBOE_OBOE_STREAM_AAUDIO_H_
#define OBOE_OBOE_STREAM_AAUDIO_H_

#include <atomic>

#include "oboe/OboeStreamBuilder.h"
#include "oboe/OboeStream.h"
#include "oboe/OboeDefinitions.h"

#include "aaudio/AAudio.h"

class AAudioLoader;

/**
 * Implementation of OboeStream that uses AAudio.
 *
 * Do not create this class directly.
 * Use an OboeStreamBuilder to create one.
 */
class OboeStreamAAudio : public OboeStream {
public:
    OboeStreamAAudio();
    explicit OboeStreamAAudio(const OboeStreamBuilder &builder);

    ~OboeStreamAAudio();

    /**
     *
     * @return true if AAudio is supported on this device.
     */
    static bool isSupported();

    // These functions override methods in OboeStream.
    // See OboeStream for documentation.
    oboe_result_t open() override;
    oboe_result_t close() override;

    oboe_result_t requestStart() override;
    oboe_result_t requestPause() override;
    oboe_result_t requestFlush() override;
    oboe_result_t requestStop() override;

    oboe_result_t write(const void *buffer,
                             int32_t numFrames,
                             int64_t timeoutNanoseconds) override;

    oboe_result_t setBufferSizeInFrames(int32_t requestedFrames) override;
    int32_t getBufferSizeInFrames() const override;
    int32_t getBufferCapacityInFrames() const override;
    int32_t getFramesPerBurst() override;
    int32_t getXRunCount() override;

    int64_t getFramesRead() override;
    int64_t getFramesWritten() override;

    oboe_result_t waitForStateChange(oboe_stream_state_t currentState,
                                             oboe_stream_state_t *nextState,
                                             int64_t timeoutNanoseconds) override;

    oboe_result_t getTimestamp(clockid_t clockId,
                                       int64_t *framePosition,
                                       int64_t *timeNanoseconds) override;

    oboe_stream_state_t getState() override;


    bool usesAAudio() const override {
        return true;
    }

public:
    aaudio_data_callback_result_t callOnAudioReady(AAudioStream *stream,
                                                   void *audioData,
                                                   int32_t numFrames);
    void callOnError(AAudioStream *stream, oboe_result_t error);

protected:
    oboe_result_t convertApplicationDataToNative(int32_t numFrames);

private:
    float              *mFloatCallbackBuffer;
    int16_t            *mShortCallbackBuffer;
    std::atomic<bool>   mCallbackThreadEnabled;
    AAudioStream       *mAAudioStream;

    static AAudioLoader *mLibLoader;
};


#endif // OBOE_OBOE_STREAM_AAUDIO_H_
