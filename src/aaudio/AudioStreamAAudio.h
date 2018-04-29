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

#ifndef OBOE_STREAM_AAUDIO_H_
#define OBOE_STREAM_AAUDIO_H_

#include <atomic>
#include <mutex>
#include <thread>

#include "aaudio/AAudio.h"

#include "oboe/AudioStreamBuilder.h"
#include "oboe/AudioStream.h"
#include "oboe/Definitions.h"

namespace oboe {

class AAudioLoader;

/**
 * Implementation of OboeStream that uses AAudio.
 *
 * Do not create this class directly.
 * Use an OboeStreamBuilder to create one.
 */
class AudioStreamAAudio : public AudioStream {
public:
    AudioStreamAAudio();
    explicit AudioStreamAAudio(const AudioStreamBuilder &builder);

    ~AudioStreamAAudio();

    /**
     *
     * @return true if AAudio is supported on this device.
     */
    static bool isSupported();

    // These functions override methods in AudioStream.
    // See AudioStream for documentation.
    Result open() override;
    Result close() override;

    Result requestStart() override;
    Result requestPause() override;
    Result requestFlush() override;
    Result requestStop() override;

    ErrorOrValue<int32_t> write(const void *buffer,
                  int32_t numFrames,
                  int64_t timeoutNanoseconds) override;

    ErrorOrValue<int32_t> read(void *buffer,
                 int32_t numFrames,
                 int64_t timeoutNanoseconds) override;

    Result setBufferSizeInFrames(int32_t requestedFrames) override;
    int32_t getBufferSizeInFrames() const override;
    int32_t getFramesPerBurst() override;
    int32_t getXRunCount() const override;

    int64_t getFramesRead() const override;
    int64_t getFramesWritten() const override;

    ErrorOrValue<double> calculateLatencyMillis() override;

    Result waitForStateChange(StreamState currentState,
                              StreamState *nextState,
                              int64_t timeoutNanoseconds) override;

    Result getTimestamp(clockid_t clockId,
                                       int64_t *framePosition,
                                       int64_t *timeNanoseconds) override;

    StreamState getState() override;

    AudioApi getAudioApi() const override {
        return AudioApi::AAudio;
    }

    DataCallbackResult callOnAudioReady(AAudioStream *stream,
                                                   void *audioData,
                                                   int32_t numFrames);

    void onErrorCallback(AAudioStream *stream, Result error);

    void onErrorInThread(AAudioStream *stream, Result error);


    void *getUnderlyingStream() const override {
        return mAAudioStream.load();
    }

protected:
    Result convertApplicationDataToNative(int32_t numFrames); // TODO remove?

private:

    float               *mFloatCallbackBuffer;
    int16_t             *mShortCallbackBuffer;
    std::atomic<bool>    mCallbackThreadEnabled;
    std::thread         *mErrorHandlingThread = nullptr;

    std::mutex           mLock; // for synchronizing start/stop/close
    std::atomic<AAudioStream *> mAAudioStream{nullptr};

    static AAudioLoader *mLibLoader;
};

} // namespace oboe

#endif // OBOE_STREAM_AAUDIO_H_
