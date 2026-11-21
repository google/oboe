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
#include <pthread.h>
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <vector>

#include <common/AdpfWrapper.h>
#include "oboe/AudioStreamBuilder.h"
#include "oboe/AudioStream.h"
#include "oboe/Definitions.h"
#include "AAudioLoader.h"

namespace oboe {

class HelperThreadTracker;

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

    virtual ~AudioStreamAAudio();

    // Called by AudioStreamBuilder after wrapping a stream (or FilterAudioStream parent) in a
    // shared_ptr. No-op for other stream types.
    static void onWeakThisSet(AudioStream *stream,
                              const std::shared_ptr<oboe::AudioStream> &sharedStream);

    // Called by ~FilterAudioStream() to close and drain an AAudio child before the parent dies.
    static void closeAndWaitForChild(AudioStream *child);

    // Registers an atexit() handler after dlopen(libaaudio.so) to close open streams before
    // libaaudio's static destructors run. Only the first call registers it.
    static void registerExitHandler();

    // For tests: number of AAudioStreamRegistry slots currently in use / allocated.
    static int32_t getSlotCountForTest();
    static int32_t getAllocatedSlotCountForTest();

    /**
     *
     * @return true if AAudio is supported on this device.
     */
    static bool isSupported();

    // These functions override methods in AudioStream.
    // See AudioStream for documentation.
    Result open() override;
    Result release() override;
    Result close() override;

    Result requestStart() override;
    Result requestPause() override;
    Result requestFlush() override;
    Result requestStop() override;
    Result stop(int64_t timeoutNanoseconds = kDefaultTimeoutNanos) override;

    ResultWithValue<int32_t> write(const void *buffer,
                  int32_t numFrames,
                  int64_t timeoutNanoseconds) override;

    ResultWithValue<int32_t> read(void *buffer,
                 int32_t numFrames,
                 int64_t timeoutNanoseconds) override;

    ResultWithValue<int32_t> setBufferSizeInFrames(int32_t requestedFrames) override;
    int32_t getBufferSizeInFrames() override;
    ResultWithValue<int32_t> getXRunCount()  override;
    bool isXRunCountSupported() const override { return true; }

    ResultWithValue<double> calculateLatencyMillis() override;

    Result waitForStateChange(StreamState currentState,
                              StreamState *nextState,
                              int64_t timeoutNanoseconds) override;

    Result getTimestamp(clockid_t clockId,
                                       int64_t *framePosition,
                                       int64_t *timeNanoseconds) override;

    StreamState getState() override;

    int32_t getDeviceId() const override;

    std::vector<int32_t> getDeviceIds() const override;

    AudioApi getAudioApi() const override {
        return AudioApi::AAudio;
    }

    oboe::DataCallbackResult callOnAudioReady(
            AAudioStream *stream,
            void *audioData,
            int32_t numFrames,
            const std::atomic<uint32_t> &slotGeneration,
            uint32_t expectedGeneration);

    int32_t callOnPartialAudioReady(
            AAudioStream *stream,
            void *audioData,
            int32_t numFrames,
            const std::atomic<uint32_t> &slotGeneration,
            uint32_t expectedGeneration);

    // Used by the Stop workaround thread (Android R and older); see launchStopThread().
    // Stops the stream unless requestStart() was called after `startGeneration` was read.
    Result requestStopFromStopThread(uint32_t startGeneration);

    bool isMMapUsed();

    void closePerformanceHint() override {
        mAdpfWrapper.close();
        mAdpfOpenAttempted = false;
    }

    oboe::Result reportWorkload(int32_t appWorkload) override {
        if (!isPerformanceHintEnabled()) {
            return oboe::Result::ErrorInvalidState;
        }
        mAdpfWrapper.reportWorkload(appWorkload);
        return oboe::Result::OK;
    }

    oboe::Result notifyWorkloadIncrease(bool cpu, bool gpu, const char* debugName) override {
        if (!isPerformanceHintEnabled()) {
            return oboe::Result::ErrorInvalidState;
        }
        return mAdpfWrapper.notifyWorkloadIncrease(cpu, gpu, debugName);
    }

    oboe::Result notifyWorkloadSpike(bool cpu, bool gpu, const char* debugName) override {
        if (!isPerformanceHintEnabled()) {
            return oboe::Result::ErrorInvalidState;
        }
        return mAdpfWrapper.notifyWorkloadSpike(cpu, gpu, debugName);
    }

    oboe::Result notifyWorkloadReset(bool cpu, bool gpu, const char* debugName) override {
        if (!isPerformanceHintEnabled()) {
            return oboe::Result::ErrorInvalidState;
        }
        return mAdpfWrapper.notifyWorkloadReset(cpu, gpu, debugName);
    }

    Result setOffloadDelayPadding(int32_t delayInFrames, int32_t paddingInFrames) override;
    ResultWithValue<int32_t> getOffloadDelay() override;
    ResultWithValue<int32_t> getOffloadPadding() override;
    Result setOffloadEndOfStream() override;

    ResultWithValue<int64_t> flushFromFrame(
            FlushFromAccuracy accuracy, int64_t positionInFrames) override;

    oboe::Result setPlaybackParameters(const PlaybackParameters& parameters) override;
    ResultWithValue<PlaybackParameters> getPlaybackParameters() override;

protected:
    static void internalErrorCallback(
            AAudioStream *stream,
            void *userData,
            aaudio_result_t error);

    static void internalPresentationEndCallback(
            AAudioStream *stream,
            void *userData);

    static void internalRoutingChangedCallback(
            AAudioStream *stream,
            void *userData,
            const int32_t *deviceIds,
            int32_t numDevices);

    void *getUnderlyingStream() const override {
        return mAAudioStream.load();
    }

    void *getCallbackUserDataForTest() const {
        return reinterpret_cast<void *>(mSlotHandle.load(std::memory_order_relaxed));
    }

    void updateFramesRead() override;
    void updateFramesWritten() override;

    void logUnsupportedAttributes();

    void beginPerformanceHintInCallback() override;

    void endPerformanceHintInCallback(int32_t numFrames) override;

    void onRoutingChanged(std::vector<int32_t> deviceIds);

    // set by callback (or app when idle)
    std::atomic<bool>    mAdpfOpenAttempted{false};
    AdpfWrapper          mAdpfWrapper;

private:
    enum class CloseMode { App, Destructor, Exit };

    void closeAndWaitForHelperThreads();
    Result closeInternal(CloseMode mode);
    static void closeAllStreamsAtExit();

    // True on Android P+ when called from this stream's own data callback (matches AAudio).
    bool isControlCallFromOwnCallbackRejected() const;

    // True if the caller is inside this stream's data/partial-data callback. Lock-free.
    bool isOnOwnDataCallbackThread() const {
        return mDataCallbackThread.load(std::memory_order_relaxed) ==
                static_cast<uintptr_t>(pthread_self());
    }

    // Must call under mLock. And stream must NOT be nullptr.
    Result requestStop_l(AAudioStream *stream);

    /**
     * Launch a thread that will stop the stream.
     */
    void launchStopThread();

    void updateDeviceIds();

    std::atomic<bool>    mCallbackThreadEnabled;
    std::atomic<bool>    mStopThreadAllowed{false};

    // pointer to the underlying 'C' AAudio stream, valid if open, null if closed
    std::atomic<AAudioStream *> mAAudioStream{nullptr};
    std::shared_mutex           mAAudioStreamLock; // to protect mAAudioStream while closing

    static AAudioLoader *mLibLoader;

    // We may not use this but it is so small that it is not worth allocating dynamically.
    AudioStreamErrorCallback mDefaultErrorCallback;

    // Tracks device ids updated by the API 37+ routing callback; never taken on the data path.
    struct UpdatedDeviceIds {
        mutable std::mutex lock;
        std::vector<int32_t> deviceIds;
        std::atomic<int32_t> primaryId{kUnspecified};
    };
    UpdatedDeviceIds mUpdatedDeviceIds;

    // pthread_self() while inside this stream's data callback, or 0.
    std::atomic<uintptr_t> mDataCallbackThread{0};
    // Set when requestStop/stop/close/release is called from own data callback.
    std::atomic<bool> mStopRequestedFromCallback{false};
    // Incremented by requestStart() so a stale Android <= R stop thread does not override it.
    std::atomic<uint32_t> mStartGeneration{0};
    // Counts active helper threads so teardown waits before freeing raw-pointer streams.
    std::shared_ptr<HelperThreadTracker> mHelperThreads;
    // Non-zero AAudioStreamRegistry handle passed as userData to all AAudio callbacks while open.
    std::atomic<uintptr_t> mSlotHandle{0};
    // Serializes closeInternal() and destructor cleanup; never taken on own data callback thread.
    std::mutex mCloseLock;
};

} // namespace oboe

#endif // OBOE_STREAM_AAUDIO_H_
