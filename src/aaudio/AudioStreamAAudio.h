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

class AAudioStreamCollection;
class HelperThreadTracker;

/**
 * Implementation of OboeStream that uses AAudio.
 *
 * Do not create this class directly.
 * Use an OboeStreamBuilder to create one.
 */
class AudioStreamAAudio : public AudioStream {
    friend class AAudioStreamCollection;
public:
    AudioStreamAAudio();
    explicit AudioStreamAAudio(const AudioStreamBuilder &builder);

    virtual ~AudioStreamAAudio();

    /**
     * Called by AudioStreamBuilder right after it wraps a new stream in a shared_ptr.
     * `stream` may be an AudioStreamAAudio or a FilterAudioStream that wraps one; any other
     * stream type is ignored. Lets error/routing/presentation callbacks (and the Stop
     * workaround thread) take shared_ptrs to the stream and its parent.
     */
    static void onWeakThisSet(AudioStream *stream,
                              const std::shared_ptr<oboe::AudioStream> &sharedStream);

    /**
     * Called by ~FilterAudioStream() with its child. If `child` is an AudioStreamAAudio, closes
     * it if still open, then waits for its Oboe helper threads, which also call
     * into the parent FilterAudioStream. No-op for other stream types. Does not require RTTI.
     */
    static void closeAndWaitForChild(AudioStream *child);

    /**
     * Called right after libaaudio.so is loaded. Registers an std::atexit() handler so that at
     * exit() or dlclose() any still-open AAudio streams (such as pre-main globals) are closed
     * while libaaudio's internal state is still intact. Only the first call registers it.
     */
    static void registerExitHandler();

    /**
     * For tests: the number of data callback slots in use (one per open stream with a data
     * callback). See DataCallbackSlots.
     */
    static int32_t getDataCallbackSlotsInUseForTest();

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

    DataCallbackResult callOnAudioReady(AAudioStream *stream, void *audioData, int32_t numFrames);

    int32_t callOnPartialAudioReady(AAudioStream *stream, void *audioData, int32_t numFrames);

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
    // Closes the stream if the app destroyed it without calling close(). Called from the
    // destructor and, via closeAndWaitForChild(), from ~FilterAudioStream().
    void cleanupUnclosedStream();

    // Who is closing the stream. Only affects whether close sleeps before AAudioStream_close().
    enum class CloseMode {
        App,         // the app called close()
        Destructor,  // the app released the stream without close()
        Exit,        // the exit handler
    };
    Result closeInternal(CloseMode mode);
    static bool needsDelayBeforeClose(CloseMode mode, StreamState stateBeforeStop);

    // Takes a DataCallbackSlots handle for this stream (see open()). False if none is free.
    bool acquireDataCallbackHandle();

    // Starting, pausing or flushing from the stream's own data callback is rejected on Android P
    // and later, before taking mLock, like AAudio does: another thread may hold mLock while AAudio
    // waits for this callback to return (for example requestStop() on an MMAP stream). Allowed
    // on O and O_MR1, where AAudio allows it too.
    bool isControlCallFromOwnCallbackRejected() const;

    // Waits until no helper thread other than the caller holds a HelperLease on
    // this stream. Returns at once if called from this stream's own data callback.
    void waitForHelperThreads();

    // True if the caller is inside this stream's callOnAudioReady()/callOnPartialAudioReady()
    // (for example, the app called close() from onAudioReady()). Lock-free.
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

private:

    std::atomic<bool>    mCallbackThreadEnabled;
    std::atomic<bool>    mStopThreadAllowed{false};

    // pointer to the underlying 'C' AAudio stream, valid if open, null if closed
    std::atomic<AAudioStream *> mAAudioStream{nullptr};
    std::shared_mutex           mAAudioStreamLock; // to protect mAAudioStream while closing

    static AAudioLoader *mLibLoader;

    // We may not use this but it is so small that it is not worth allocating dynamically.
    AudioStreamErrorCallback mDefaultErrorCallback;

    // With AAudio introduce routing changed callback in Android 17, aaudio may not disconnect
    // the stream when routing is changed. In that case, UpdatedDeviceIds is introduced to track
    // the device ids. The routing callback writes them and getDeviceId()/getDeviceIds() read
    // them on other threads, so they are guarded by a mutex (never taken on the data path).
    struct UpdatedDeviceIds {
        mutable std::mutex lock;
        std::vector<int32_t> deviceIds;
    };
    UpdatedDeviceIds mUpdatedDeviceIds;

    // Copy of this stream's AAudioStreamCollection registration, for launchStopThread(), which
    // runs on the data callback thread and must not take the registry lock. Written only
    // during openStream(), before the stream can start. The *Set flags are true if the
    // stream / its FilterAudioStream parent is owned by a shared_ptr.
    bool mWeakThisSet = false;
    std::weak_ptr<AudioStream> mWeakParentStream;
    bool mWeakParentSet = false;

    // pthread_self() of the thread currently inside callOnAudioReady()/callOnPartialAudioReady(),
    // or 0. Only the callback thread writes it, and readers only compare it with their own
    // pthread_self(). A thread can only match a value it stored itself, so relaxed ordering
    // is enough.
    std::atomic<uintptr_t> mDataCallbackThread{0};

    // Set when the app calls requestStop(), stop(), close() or release() from inside this
    // stream's data callback. Those calls then return without locking (requestStop()/stop()
    // return OK; close()/release() return ErrorInvalidState), and the callback returns
    // DataCallbackResult::Stop when the app's onAudioReady() returns. Cleared by requestStart().
    std::atomic<bool> mStopRequestedFromCallback{false};

    // Incremented by requestStart() under mLock. The Stop workaround thread (Android R and
    // older) only stops the stream if no requestStart() happened after the callback asked to
    // stop, so a later start is not lost.
    std::atomic<uint32_t> mStartGeneration{0};

    // Counts the Oboe helper threads that are using this stream, so the destructor can wait for
    // them. Shared with the registry entry and with each HelperLease, so it outlives the stream.
    std::shared_ptr<HelperThreadTracker> mHelperThreads;

    // userData of the AAudio data callback while the stream is open (see DataCallbackSlots).
    uintptr_t mDataCallbackHandle = 0;
    bool mHasDataCallbackHandle = false;

    // Held for the whole of close(). Never taken by this stream's own data callback.
    std::mutex mCloseLock;
};

} // namespace oboe

#endif // OBOE_STREAM_AAUDIO_H_
