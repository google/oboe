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

#include <algorithm>
#include <cassert>
#include <chrono>
#include <pthread.h>
#include <unordered_map>
#include <vector>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "aaudio/AAudioLoader.h"
#include "aaudio/AudioStreamAAudio.h"
#include "common/OboeDebug.h"
#include "oboe/AudioClock.h"
#include "oboe/Utilities.h"
#include "AAudioExtensions.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>
#include <common/QuirksManager.h>

#endif

#ifndef OBOE_FIX_FORCE_STARTING_TO_STARTED
// Workaround state problems in AAudio
// TODO Which versions does this occur in? Verify fixed in Q.
#define OBOE_FIX_FORCE_STARTING_TO_STARTED 1
#endif // OBOE_FIX_FORCE_STARTING_TO_STARTED

using namespace oboe;
AAudioLoader *AudioStreamAAudio::mLibLoader = nullptr;

namespace {
// Requests a stop, skipping it on O_MR1 and older if the stream is already stopping or stopped,
// because those versions return a state-machine error in that case.
Result requestStopChecked(AAudioLoader *libLoader, AAudioStream *stream) {
    if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
        StreamState state = static_cast<StreamState>(libLoader->stream_getState(stream));
        if (state == StreamState::Stopping || state == StreamState::Stopped) {
            return Result::OK;
        }
    }
    return static_cast<Result>(libLoader->stream_requestStop(stream));
}

// Error, routing and presentation callbacks hold shared_ptrs to keep the stream alive while
// they run. If one of them dropped the LAST reference, ~AudioStreamAAudio() would run on an
// AAudio callback thread and call AAudioStream_close() there, which AAudio does not allow
// and which can deadlock. So any references still held at the end of such a callback are
// released on a short-lived Oboe thread instead.
void releaseRefsOffCallbackThread(std::shared_ptr<oboe::AudioStream> &&stream,
                                  std::shared_ptr<oboe::AudioStream> &&parent) {
    if (!stream && !parent) {
        return;
    }
    std::thread([s = std::move(stream), p = std::move(parent)]() mutable {
        s.reset();
        p.reset();
    }).detach();
}
} // namespace

namespace oboe {

namespace {
// The tracker whose lease the current Oboe helper thread holds, if any. Lets a destructor that
// runs on a helper thread (for example the app deletes the stream in onErrorAfterClose()) skip
// waiting for itself. Read only by helper threads and by a destructor that finds helpers active.
thread_local const HelperThreadTracker *tAttachedTracker = nullptr;
} // namespace

/**
 * Counts the Oboe helper threads (error, routing, presentation and stop threads) that are using
 * one stream, so that the stream's destructor can wait for them instead of freeing the stream
 * while they still use it. This is what protects streams opened with a raw pointer.
 *
 * Lock-free. Owned through a shared_ptr by the stream, its registry entry and every lease, so it
 * stays valid even after the stream itself has been freed.
 */
class HelperThreadTracker {
public:
    void acquire() {
        mCount.fetch_add(1, std::memory_order_relaxed);
    }

    // Release ordering: the helper's last use of the stream happens before the destructor sees
    // the lower count and frees the stream.
    void release() {
        mCount.fetch_sub(1, std::memory_order_release);
    }

    // Waits up to `timeout` until no helper thread other than the calling thread holds a lease.
    // Polls, because deleting a stream is rare and this keeps the tracker lock-free.
    // Returns false on timeout.
    bool waitForOtherThreads(std::chrono::milliseconds timeout) const {
        // Common case: no helper holds a lease; avoids touching thread_local on non-helper threads.
        if (mCount.load(std::memory_order_acquire) == 0) {
            return true;
        }
        const int own = (tAttachedTracker == this) ? 1 : 0;
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (mCount.load(std::memory_order_acquire) > own) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

private:
    std::atomic<int> mCount{0};  // leases held, including helpers not started yet
};

/**
 * Move-only RAII lease on a stream's HelperThreadTracker. Take it before starting a helper thread
 * that will use the stream, move it into that thread, and release it after the thread's last use
 * of the stream. While any lease is held, ~AudioStreamAAudio() waits before freeing the stream.
 */
class HelperLease {
public:
    HelperLease() = default;
    explicit HelperLease(std::shared_ptr<HelperThreadTracker> tracker)
            : mTracker(std::move(tracker)) {
        if (mTracker) mTracker->acquire();
    }
    HelperLease(HelperLease &&other) noexcept
            : mTracker(std::move(other.mTracker)), mAttached(other.mAttached) {
        other.mAttached = false;
    }
    HelperLease(const HelperLease &) = delete;
    HelperLease &operator=(const HelperLease &) = delete;
    HelperLease &operator=(HelperLease &&other) noexcept {
        if (this != &other) {
            reset();
            mTracker = std::move(other.mTracker);
            mAttached = other.mAttached;
            other.mAttached = false;
        }
        return *this;
    }
    ~HelperLease() { reset(); }

    // Call first thing on the helper thread.
    void attachToCurrentThread() {
        if (mTracker && !mAttached) {
            tAttachedTracker = mTracker.get();
            mAttached = true;
        }
    }

    // Call after the helper thread's last use of the stream.
    void reset() {
        if (mTracker) {
            if (mAttached) {
                tAttachedTracker = nullptr;
                mAttached = false;
            }
            mTracker->release();
            mTracker.reset();
        }
    }

private:
    std::shared_ptr<HelperThreadTracker> mTracker;
    bool mAttached = false;
};

/**
 * A singleton that tracks every open AudioStreamAAudio. A stream is added when open()
 * succeeds and removed when it is closed or destroyed, before its memory is freed.
 *
 * The error, routing and presentation callbacks receive a raw pointer that may already be
 * freed, so they look the stream up here instead of dereferencing it. For each stream the
 * registry stores:
 * - the AAudioStream*, to reject a late callback for a freed stream whose address was reused;
 * - weak_ptrs to the stream and to any FilterAudioStream parent, so a callback can take
 *   shared_ptrs that keep them alive if the app owns them through shared_ptrs;
 * - the stream's HelperThreadTracker, so that the lookup and taking a HelperLease happen
 *   under one lock. The lease makes the stream's destructor wait, which also protects
 *   streams opened with the raw-pointer openStream(AudioStream**).
 *
 * The data callbacks intentionally do NOT use this class, because the data path must stay
 * lock-free. They are safe because every teardown path calls close() before the stream
 * object is freed, and close() returns only when no data callback can reach the stream
 * (see DataCallbackSlots). The one exception is an app that destroys the stream from
 * inside its own data callback; see cleanupUnclosedStream().
 */
class AAudioStreamCollection {
public:
    static AAudioStreamCollection &getInstance() {
        // Intentionally leaked so it outlives any static/global AudioStream destructors
        // that run during exit() or dlclose().
        static AAudioStreamCollection *instance = new AAudioStreamCollection();
        return *instance;
    }

    AAudioStreamCollection(const AAudioStreamCollection &) = delete;
    AAudioStreamCollection &operator=(const AAudioStreamCollection &) = delete;
    AAudioStreamCollection(AAudioStreamCollection &&) = delete;
    AAudioStreamCollection &operator=(AAudioStreamCollection &&) = delete;

    // Called from open() on the app thread. Reads stream->getParentStream() without mLock, so
    // it must not run concurrently with teardown of `stream`.
    void addStream(AudioStreamAAudio *stream,
                   AAudioStream *aaudioStream,
                   const std::weak_ptr<oboe::AudioStream> &weakStream = {},
                   bool usesSharedPtr = false,
                   const std::weak_ptr<oboe::AudioStream> &weakParentStream = {},
                   bool usesParentSharedPtr = false) {
        const AudioStream *parentStream =
                stream->hasParentStream() ? stream->getParentStream() : nullptr;
        std::lock_guard<std::mutex> lock(mLock);
        mStreams[static_cast<const AudioStream *>(stream)] = StreamInfo{
                stream,
                parentStream,
                aaudioStream,
                weakStream,
                weakParentStream,
                usesSharedPtr,
                usesParentSharedPtr,
                stream->mHelperThreads
        };
    }

    // Called by AudioStreamBuilder right after it wraps a new stream in a shared_ptr, before
    // the stream can start. `stream` may be an AudioStreamAAudio or a FilterAudioStream that
    // wraps one; any other stream is ignored. Reads stream->getParentStream() without mLock.
    void onWeakThisSet(AudioStream *stream,
                       const std::shared_ptr<oboe::AudioStream> &sharedStream) {
        if (stream == nullptr) {
            return;
        }
        // Read the parent link on the owning thread, outside mLock.
        const AudioStream *parentStream =
                stream->hasParentStream() ? stream->getParentStream() : nullptr;
        std::lock_guard<std::mutex> lock(mLock);
        auto it = mStreams.find(stream);
        if (it != mStreams.end()) {
            const bool usesShared = (sharedStream != nullptr);
            it->second.parentStream = parentStream;
            it->second.weakStream = sharedStream;
            it->second.usesSharedPtr = usesShared;
            it->second.oboeStream->mWeakThisSet = usesShared;
            return;
        }

        // Otherwise `stream` may be a parent FilterAudioStream wrapping an open AAudio child.
        // Compare cached pointer identity only; never dereference other streams.
        for (auto &[key, info] : mStreams) {
            if (info.parentStream == stream) {
                const bool usesParentShared = (sharedStream != nullptr);
                info.weakParentStream = sharedStream;
                info.usesParentSharedPtr = usesParentShared;
                info.oboeStream->mWeakParentStream = sharedStream;
                info.oboeStream->mWeakParentSet = usesParentShared;
            }
        }
    }

    void removeStream(const AudioStream *stream) {
        std::lock_guard<std::mutex> lock(mLock);
        mStreams.erase(stream);
    }

    // Closes every still-registered stream. Called during exit() or dlclose() before libaaudio's
    // static destructors run (see AAudioLoader::open()). Never calls stream destructors or
    // close() while holding mLock.
    void closeAllStreams() {
        while (true) {
            AudioStreamAAudio *oboeStream = nullptr;
            std::shared_ptr<oboe::AudioStream> sharedStream;
            std::shared_ptr<oboe::AudioStream> sharedParentStream;
            HelperLease lease;
            bool alive = true;
            {
                std::lock_guard<std::mutex> lock(mLock);
                if (mStreams.empty()) {
                    return;
                }
                auto it = mStreams.begin();
                StreamInfo info = std::move(it->second);
                mStreams.erase(it);

                if (info.usesParentSharedPtr) {
                    sharedParentStream = info.weakParentStream.lock();
                    if (!sharedParentStream) {
                        alive = false;
                    }
                }
                if (alive && info.usesSharedPtr) {
                    sharedStream = info.weakStream.lock();
                    if (!sharedStream) {
                        alive = false;
                    }
                }
                if (alive) {
                    oboeStream = info.oboeStream;
                    lease = HelperLease(info.helperThreads);
                }
            }
            if (alive && oboeStream != nullptr && !oboeStream->isOnOwnDataCallbackThread()) {
                LOGW("AudioStreamAAudio(%p) still open at exit; closing stream now.", oboeStream);
                oboeStream->closeInternal(AudioStreamAAudio::CloseMode::Exit);
            }
            lease.reset();
        }
    }

    /**
     * What an error/routing/presentation callback gets back from getStream().
     * If `alive` is false the stream is closed or being destroyed and must not be touched, but
     * `stream`/`parentStream` may still hold a reference that must be released off the AAudio
     * thread (see releaseRefsOffCallbackThread()).
     */
    struct CallbackRefs {
        bool alive = false;
        std::shared_ptr<oboe::AudioStream> stream;        // set if the app owns it by shared_ptr
        std::shared_ptr<oboe::AudioStream> parentStream;  // set if wrapped and parent is shared
        HelperLease lease;  // makes the stream's destructor wait until the callback work is done
    };

    /**
     * Looks up a stream from an error/routing/presentation callback.
     *
     * Never dereferences `stream`. Returns `alive == false` if the stream is not registered
     * (closed or being destroyed), if `aaudioStream` does not match the registered AAudioStream
     * (a stale callback for a freed stream whose address was reused), or if the app's
     * shared_ptr to the stream or to its FilterAudioStream parent has already expired.
     * Otherwise it also returns a HelperLease, taken under the same lock as the lookup, so the
     * stream cannot be freed until the lease is released.
     */
    CallbackRefs getStream(AudioStreamAAudio *stream, AAudioStream *aaudioStream) {
        CallbackRefs refs;
        if (stream == nullptr) {
            return refs;
        }
        std::lock_guard<std::mutex> lock(mLock);
        auto it = mStreams.find(static_cast<const AudioStream *>(stream));
        if (it == mStreams.end()) {
            return refs;
        }
        const StreamInfo &info = it->second;
        if (info.aaudioStream != aaudioStream) {
            return refs;
        }
        // No stream destructor may run while mLock is held: it would call back into this
        // registry and deadlock. So any shared_ptr locked here is returned to the caller, even
        // when `alive` is false, and the caller releases it off the AAudio thread.
        if (info.usesParentSharedPtr) {
            refs.parentStream = info.weakParentStream.lock();
            if (!refs.parentStream) {
                return refs;
            }
        }
        if (info.usesSharedPtr) {
            refs.stream = info.weakStream.lock();
            if (!refs.stream) {
                return refs;
            }
        }
        refs.alive = true;
        refs.lease = HelperLease(info.helperThreads);
        return refs;
    }

private:
    struct StreamInfo {
        AudioStreamAAudio *oboeStream = nullptr;
        // Identity of the wrapping FilterAudioStream, if any. Compared only, never dereferenced.
        const AudioStream *parentStream = nullptr;
        AAudioStream *aaudioStream = nullptr;
        std::weak_ptr<oboe::AudioStream> weakStream;
        std::weak_ptr<oboe::AudioStream> weakParentStream;
        // True if the app owns the stream (or its parent) through a shared_ptr. Then the
        // matching weak_ptr must lock successfully for the stream to be considered alive.
        bool usesSharedPtr = false;
        bool usesParentSharedPtr = false;
        std::shared_ptr<HelperThreadTracker> helperThreads;
    };

    // Private constructor to prevent direct instantiation
    AAudioStreamCollection() = default;

    std::mutex mLock;
    std::unordered_map<const AudioStream *, StreamInfo> mStreams;
};

/**
 * Maps the userData of AAudio data callbacks to the AudioStreamAAudio, without ever giving AAudio
 * a pointer to memory that may be freed.
 *
 * On Legacy (non-MMAP) streams, AAudioStream_close() does not reliably wait for the
 * AudioTrack/AudioRecord callback thread (before Android S it never does, and later versions
 * cannot be fully ruled out), so a data callback can arrive after close() returns and the app
 * has deleted the stream.
 *
 * So AAudio gets a handle *value* as userData: a slot index plus the slot's generation. The slots
 * are never freed. close() calls release() right after AAudioStream_close(): it clears the slot,
 * bumps its generation and waits for running callbacks. Any later callback, even one that
 * arrives after the slot has been given to another stream, finds a different generation and
 * returns Stop without touching any stream. The slots are one fixed, zero-initialized static
 * array (24 KB on 64-bit ABIs, of which only the pages of slots actually used are touched), so
 * nothing is allocated or leaked per open.
 *
 * The data callback side is lock-free and wait-free: one atomic increment, two loads and one
 * decrement. acquire() and release() run on the control plane and take a mutex.
 */
class DataCallbackSlots {
public:
    struct Slot {
        std::atomic<uint32_t> generation{0};
        std::atomic<AudioStreamAAudio *> stream{nullptr};
        std::atomic<int32_t> active{0};  // data callbacks currently using this slot
        bool inUse = false;              // guarded by getLock()
    };

    // Takes a free slot for `stream`. Returns false if every slot is in use.
    static bool acquire(AudioStreamAAudio *stream, uintptr_t *handle) {
        std::lock_guard<std::mutex> lock(getLock());
        // The lowest free index, so the memory touched is bounded by the peak number of
        // streams open at the same time.
        for (uint32_t index = 0; index < kMaxSlots; index++) {
            Slot &slot = sSlots[index];
            if (!slot.inUse) {
                slot.inUse = true;
                slot.stream.store(stream);
                const uintptr_t generation = slot.generation.load() & kGenerationMask;
                *handle = (generation << kIndexBits) | index;
                return true;
            }
        }
        return false;
    }

    // Data callback: returns the stream and counts this callback as active, or nullptr if the
    // handle is stale (the stream was closed). Always call leave(slot) afterwards.
    static AudioStreamAAudio *enter(void *userData, Slot **slot) {
        const auto handle = reinterpret_cast<uintptr_t>(userData);
        *slot = &sSlots[handle & kIndexMask];
        // seq_cst here and in release(): either this callback sees the new generation, or
        // release() sees it as active and waits for it.
        (*slot)->active.fetch_add(1);
        if (((*slot)->generation.load() & kGenerationMask) != (handle >> kIndexBits)) {
            return nullptr;
        }
        return (*slot)->stream.load();
    }

    static void leave(Slot *slot) {
        slot->active.fetch_sub(1, std::memory_order_release);
    }

    // Invalidates `handle` and frees its slot. Afterwards no data callback uses the stream, and
    // none ever will. `waitForCallbacks` is false only when no callback can be running or on
    // the stream's own data callback thread; a later owner of the slot then waits for that
    // callback in its own release().
    static void release(uintptr_t handle, bool waitForCallbacks) {
        Slot &slot = sSlots[handle & kIndexMask];
        slot.stream.store(nullptr);
        slot.generation.fetch_add(1);
        if (waitForCallbacks) {
            // A callback normally finishes within microseconds; sleep if it blocks.
            for (int spins = 0; slot.active.load() != 0; ++spins) {
                if (spins < 100) {
                    std::this_thread::yield();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
        std::lock_guard<std::mutex> lock(getLock());
        slot.inUse = false;
    }

    // For tests: the number of slots in use now.
    static int32_t getInUseCountForTest() {
        std::lock_guard<std::mutex> lock(getLock());
        int32_t count = 0;
        for (const Slot &slot : sSlots) {
            count += slot.inUse ? 1 : 0;
        }
        return count;
    }

private:
    // 10 index bits: up to 1024 streams open at once. The rest of the handle is the generation:
    // 22 bits on 32-bit ABIs (about 4 million reuses of one slot before it wraps), 32 bits on
    // 64-bit ABIs. Late callbacks arrive within milliseconds of close(), so wrapping is harmless.
    static constexpr uint32_t kIndexBits = 10;
    static constexpr uint32_t kMaxSlots = 1u << kIndexBits;
    static constexpr uintptr_t kIndexMask = kMaxSlots - 1;
    static constexpr uintptr_t kGenerationMask = (sizeof(uintptr_t) >= 8)
            ? uintptr_t{0xFFFFFFFFu}
            : ((uintptr_t{1} << (32 - kIndexBits)) - 1);

    // Static storage that is never freed, so a late callback always reads valid memory.
    // Constant-initialized with a trivial destructor, so it is also safe during exit().
    static Slot sSlots[kMaxSlots];

    static std::mutex &getLock() {
        // Intentionally leaked, like the other singletons, so streams closed during exit()
        // never use a destroyed mutex.
        static std::mutex *lock = new std::mutex();
        return *lock;
    }
};

DataCallbackSlots::Slot DataCallbackSlots::sSlots[DataCallbackSlots::kMaxSlots];

int32_t AudioStreamAAudio::getDataCallbackSlotsInUseForTest() {
    return DataCallbackSlots::getInUseCountForTest();
}

} // namespace oboe

// 'C' wrapper for the data callback method.
//
// This is the real-time data path, including callOnAudioReady() and fireDataCallback().
// Keep it lock-free and allocation-free: no registry lookups, mutexes, shared_ptr copies,
// thread_local, or logging while the callback returns Continue. Existing exceptions: the
// Stop workaround on Android R and older (launchStopThread()) and ADPF (AdpfWrapper), which
// opens its session on the first callback after a start.
//
// The stream cannot be freed while this runs: close() releases the stream's DataCallbackSlots
// handle and waits for running callbacks before it returns, on every Android version. The
// exception is an app that releases its last reference to the stream inside onAudioReady(); that
// is not supported and may crash (see cleanupUnclosedStream()). To stop from the callback, return
// DataCallbackResult::Stop (or call requestStop()) and release the stream from another thread.
static aaudio_data_callback_result_t oboe_aaudio_data_callback_proc(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {
    DataCallbackSlots::Slot *slot = nullptr;
    AudioStreamAAudio *oboeStream = DataCallbackSlots::enter(userData, &slot);
    const DataCallbackResult result = (oboeStream != nullptr)
            ? oboeStream->callOnAudioReady(stream, audioData, numFrames)
            : DataCallbackResult::Stop;  // closed: never touch the stream
    DataCallbackSlots::leave(slot);
    return static_cast<aaudio_data_callback_result_t>(result);
}

// 'C' wrapper for the partial data callback method. Same rules as above.
static int32_t oboe_aaudio_partial_data_callback_proc(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {
    DataCallbackSlots::Slot *slot = nullptr;
    AudioStreamAAudio *oboeStream = DataCallbackSlots::enter(userData, &slot);
    // A negative result stops the stream.
    const int32_t result = (oboeStream != nullptr)
            ? oboeStream->callOnPartialAudioReady(stream, audioData, numFrames)
            : -1;
    DataCallbackSlots::leave(slot);
    return result;
}

// This runs in its own thread.
// Only one of these threads will be launched from internalErrorCallback().
// It calls app error callbacks from a static function in case the stream gets deleted.
static void oboe_aaudio_error_thread_proc_common(AudioStreamAAudio *oboeStream,
                                          Result error) {
#if 0
    LOGE("%s() sleep for 5 seconds", __func__);
    usleep(5*1000*1000);
    LOGD("%s() - woke up -------------------------", __func__);
#endif
    AudioStreamErrorCallback *errorCallback = oboeStream->getErrorCallback();
    if (errorCallback == nullptr) return; // should be impossible
    bool isErrorHandled = errorCallback->onError(oboeStream, error);

    if (!isErrorHandled) {
        oboeStream->requestStop();
        errorCallback->onErrorBeforeClose(oboeStream, error);
        oboeStream->close();
        // Warning, oboeStream may get deleted by this callback.
        errorCallback->onErrorAfterClose(oboeStream, error);
    }
}

// Every helper thread below receives a HelperLease. It attaches the lease first, so that a
// destructor running on this same thread does not wait for itself, and it releases the lease
// after its last use of the stream. Until then, deleting the stream on another thread waits.

// Callback thread for raw pointers.
static void oboe_aaudio_error_thread_proc(AudioStreamAAudio *oboeStream,
                                          Result error,
                                          HelperLease lease) {
    lease.attachToCurrentThread();
    LOGD("%s(,%d) - entering >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __func__, error);
    oboe_aaudio_error_thread_proc_common(oboeStream, error);
    // oboeStream may have been deleted by onErrorAfterClose(); do not use it below.
    lease.reset();
    LOGD("%s() - exiting <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", __func__);
}

// Callback thread for shared pointers.
static void oboe_aaudio_error_thread_proc_shared(std::shared_ptr<AudioStream> sharedStream,
                                          std::shared_ptr<AudioStream> sharedParentStream,
                                          Result error,
                                          HelperLease lease) {
    lease.attachToCurrentThread();
    LOGD("%s(,%d) - entering >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __func__, error);
    // Hold the shared pointer(s) while we use the raw pointer.
    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(sharedStream.get());
    oboe_aaudio_error_thread_proc_common(oboeStream, error);
    lease.reset();
    LOGD("%s() - exiting <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", __func__);
}

static void oboe_aaudio_presentation_thread_proc_common(AudioStreamAAudio *oboeStream) {
    auto presentationCallback = oboeStream->getPresentationCallback();
    if (presentationCallback == nullptr) return; // should be impossible
    presentationCallback->onPresentationEnded(oboeStream);
}

// Callback thread for raw pointers
static void oboe_aaudio_presentation_thread_proc(AudioStreamAAudio *oboeStream,
                                                 HelperLease lease) {
    lease.attachToCurrentThread();
    LOGD("%s() - entering >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __func__);
    oboe_aaudio_presentation_thread_proc_common(oboeStream);
    lease.reset();
    LOGD("%s() - exiting <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", __func__);
}

// Callback thread for shared pointers
static void oboe_aaudio_presentation_end_thread_proc_shared(
        std::shared_ptr<AudioStream> sharedStream,
        std::shared_ptr<AudioStream> sharedParentStream,
        HelperLease lease) {
    lease.attachToCurrentThread();
    LOGD("%s() - entering >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __func__);
    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(sharedStream.get());
    oboe_aaudio_presentation_thread_proc_common(oboeStream);
    lease.reset();
    LOGD("%s() - exiting <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", __func__);
}

static void oboe_aaudio_routing_changed_thread_proc_common(
        AudioStreamAAudio *oboeStream, const int32_t *deviceIds, int32_t numDevices) {
    auto routingCallback = oboeStream->getRoutingCallback();
    if (routingCallback == nullptr) return;
    routingCallback->onRoutingChanged(oboeStream, deviceIds, numDevices);
}

// Callback thread for raw pointers
static void oboe_aaudio_routing_changed_thread_proc(
        AudioStreamAAudio *oboeStream, std::vector<int32_t> deviceIds, HelperLease lease) {
    lease.attachToCurrentThread();
    LOGD("%s() - entering >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __func__);
    oboe_aaudio_routing_changed_thread_proc_common(oboeStream, deviceIds.data(),
                                                   static_cast<int32_t>(deviceIds.size()));
    lease.reset();
    LOGD("%s() - exiting <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", __func__);
}

// Callback thread for shared pointers
static void oboe_aaudio_routing_changed_thread_proc_shared(
        std::shared_ptr<AudioStream> sharedStream,
        std::shared_ptr<AudioStream> sharedParentStream,
        std::vector<int32_t> deviceIds,
        HelperLease lease) {
    lease.attachToCurrentThread();
    LOGD("%s() - entering >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __func__);
    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(sharedStream.get());
    oboe_aaudio_routing_changed_thread_proc_common(oboeStream, deviceIds.data(),
                                                   static_cast<int32_t>(deviceIds.size()));
    lease.reset();
    LOGD("%s() - exiting <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", __func__);
}

namespace oboe {

/*
 * Create a stream that uses Oboe Audio API.
 */
AudioStreamAAudio::AudioStreamAAudio(const AudioStreamBuilder &builder)
    : AudioStream(builder)
    , mAAudioStream(nullptr) {
    mCallbackThreadEnabled.store(false);
    mLibLoader = AAudioLoader::getInstance();
    mHelperThreads = std::make_shared<HelperThreadTracker>();
}

AudioStreamAAudio::~AudioStreamAAudio() {
    // Close first. After that no data callback is running, the stream is no longer in the
    // registry, and so no new HelperLease can be taken. Then wait for the helper threads that
    // already hold one, so they never use a freed stream.
    cleanupUnclosedStream();
    waitForHelperThreads();
}

void AudioStreamAAudio::waitForHelperThreads() {
    if (!mHelperThreads || isOnOwnDataCallbackThread()) {
        // Never block the real-time callback thread if the app released its last reference there.
        return;
    }
    // App callbacks normally return quickly. Freeing the stream while one is still running would
    // let it use freed memory, so wait for as long as it takes and log while waiting.
    constexpr std::chrono::milliseconds kWarnAfter{500};
    constexpr std::chrono::milliseconds kWarnEvery{2000};
    if (mHelperThreads->waitForOtherThreads(kWarnAfter)) {
        return;
    }
    int64_t waitedMillis = kWarnAfter.count();
    do {
        LOGE("AudioStreamAAudio(%p) deleted while an Oboe callback thread still uses it; "
             "waited %lld ms for the app's error/routing/presentation callback to return. "
             "Is that callback blocked on the thread deleting the stream?",
             this, static_cast<long long>(waitedMillis));
        waitedMillis += kWarnEvery.count();
    } while (!mHelperThreads->waitForOtherThreads(kWarnEvery));
}

void AudioStreamAAudio::closeAndWaitForChild(AudioStream *child) {
    // Only called with a FilterAudioStream's child, which is never itself a FilterAudioStream,
    // so AudioApi::AAudio uniquely identifies AudioStreamAAudio without RTTI.
    if (child != nullptr && child->getAudioApi() == AudioApi::AAudio) {
        auto *aaudioChild = static_cast<AudioStreamAAudio *>(child);
        aaudioChild->cleanupUnclosedStream();
        aaudioChild->waitForHelperThreads();
    }
}

namespace {
// The process that registered closeAllStreamsAtExit(). A child forked from it that calls exit()
// must not close the AAudio streams it inherited from the parent.
std::atomic<pid_t> sExitHandlerPid{0};

void closeAllStreamsAtExit() {
    if (getpid() != sExitHandlerPid.load()) {
        return;
    }
    AAudioStreamCollection::getInstance().closeAllStreams();
}
} // namespace

void AudioStreamAAudio::registerExitHandler() {
    // Registered after dlopen(libaaudio.so), so at exit() it runs in reverse order BEFORE
    // libaaudio's own static destructors tear down internal AAudio state. Closes any open
    // streams (such as pre-main globals) whose destructors would otherwise run too late.
    static const bool sRegistered = [] {
        sExitHandlerPid.store(getpid());
        return std::atexit(&closeAllStreamsAtExit) == 0;
    }();
    (void) sRegistered;
}

void AudioStreamAAudio::onWeakThisSet(AudioStream *stream,
                                      const std::shared_ptr<oboe::AudioStream> &sharedStream) {
    AAudioStreamCollection::getInstance().onWeakThisSet(stream, sharedStream);
}

void AudioStreamAAudio::cleanupUnclosedStream() {
    // close() unregisters before nulling mAAudioStream, so null means nothing left to clean up.
    if (mAAudioStream.load() == nullptr) {
        return;
    }
    if (!isOnOwnDataCallbackThread()) {
        LOGW("AudioStreamAAudio(%p) destroyed without close()! Closing stream now.", this);
        closeInternal(CloseMode::Destructor);
        return;
    }

    // The app released its last reference inside its own onAudioReady() (unsupported).
    // AAudio rejects closing a stream from its own callback and blocking there would stall the
    // real-time thread, so unregister now and stop/close the AAudioStream on a separate thread.
    // The app may still crash because this object is freed while its callback is running.
    AAudioStreamCollection::getInstance().removeStream(this);
    AAudioStream *stream = mAAudioStream.exchange(nullptr);
    if (stream == nullptr) {
        return;
    }
    // Later callbacks return Stop. This callback is still running, so do not wait for it.
    if (mHasDataCallbackHandle) {
        DataCallbackSlots::release(mDataCallbackHandle, /*waitForCallbacks=*/false);
        mHasDataCallbackHandle = false;
    }
    closePerformanceHint();  // AudioStream::close() is bypassed here.
    LOGW("AudioStreamAAudio(%p) destroyed from its own data callback without close(). "
         "This is unsupported; closing the AAudioStream asynchronously.", this);
    AAudioLoader *libLoader = mLibLoader;
    const bool workarounds = OboeGlobals::areWorkaroundsEnabled();
    const int32_t delayMillis = std::min<int32_t>(mDelayBeforeCloseMillis,
                                                  kMaxDelayBeforeCloseMillis);
    std::thread([libLoader, stream, workarounds, delayMillis]() {
        // Always stop first (see closeInternal()).
        requestStopChecked(libLoader, stream);
        if (workarounds && delayMillis > 0) {
            usleep(static_cast<useconds_t>(delayMillis) * 1000);
        }
        libLoader->stream_close(stream);
    }).detach();
}

bool AudioStreamAAudio::isSupported() {
    mLibLoader = AAudioLoader::getInstance();
    int openResult = mLibLoader->open();
    return openResult == 0;
}

// Static method for the error callback.
// We use a method so we can access protected methods on the stream.
// Launch a thread to handle the error.
// That other thread can safely stop, close and delete the stream.
void AudioStreamAAudio::internalErrorCallback(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error) {
    oboe::Result oboeResult = static_cast<Result>(error);
    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(userData);

    // Coerce the error code if needed to workaround a regression in RQ1A that caused
    // the wrong code to be passed when headsets plugged in. See b/173928197.
    if (OboeGlobals::areWorkaroundsEnabled()
            && getSdkVersion() == __ANDROID_API_R__
            && oboeResult == oboe::Result::ErrorTimeout) {
        oboeResult = oboe::Result::ErrorDisconnected;
        LOGD("%s() ErrorTimeout changed to ErrorDisconnected to fix b/173928197", __func__);
    }

    // Keeps the stream alive while the error is handled: a shared_ptr if the app uses
    // AudioStreamBuilder::openStream(shared_ptr), and a HelperLease in every case.
    auto [isStreamAlive, sharedStream, sharedParentStream, lease] =
            AAudioStreamCollection::getInstance().getStream(oboeStream, stream);
    if (!isStreamAlive) {
        // The stream is already closed. No need to call error callback.
        releaseRefsOffCallbackThread(std::move(sharedStream), std::move(sharedParentStream));
        return;
    }

    oboeStream->mErrorCallbackResult = oboeResult;

    // These checks should be enough because we assume that the stream close()
    // will join() any active callback threads and will not allow new callbacks.
    if (oboeStream->wasErrorCallbackCalled()) { // block extra error callbacks
        LOGE("%s() multiple error callbacks called!", __func__);
    } else if (stream != oboeStream->getUnderlyingStream()) {
        LOGW("%s() stream already closed or closing", __func__); // might happen if there are bugs
    } else if (sharedStream) {
        // Handle error on a separate thread using shared pointer. Move the refs so this
        // AAudio thread never drops the last one.
        std::thread t(oboe_aaudio_error_thread_proc_shared, std::move(sharedStream),
                      std::move(sharedParentStream), oboeResult, std::move(lease));
        t.detach();
    } else {
        // Handle error on a separate thread.
        std::thread t(oboe_aaudio_error_thread_proc, oboeStream, oboeResult, std::move(lease));
        t.detach();
    }
    releaseRefsOffCallbackThread(std::move(sharedStream), std::move(sharedParentStream));
}

void AudioStreamAAudio::beginPerformanceHintInCallback() {
    if (isPerformanceHintEnabled()) {
        if (!mAdpfOpenAttempted) {
            int64_t targetDurationNanos = (mFramesPerBurst * 1e9) / getSampleRate();
            // This has to be called from the callback thread so we get the right TID.
            int adpfResult = mAdpfWrapper.open(gettid(), targetDurationNanos);
            if (adpfResult < 0) {
                LOGW("WARNING ADPF not supported, %d\n", adpfResult);
            } else {
                LOGD("ADPF is now active\n");
            }
            mAdpfOpenAttempted = true;
        }
        mAdpfWrapper.onBeginCallback();
    } else if (!isPerformanceHintEnabled() && mAdpfOpenAttempted) {
        LOGD("ADPF closed\n");
        mAdpfWrapper.close();
        mAdpfOpenAttempted = false;
    }
}

void AudioStreamAAudio::endPerformanceHintInCallback(int32_t numFrames) {
    if (mAdpfWrapper.isOpen()) {
        // Scale the measured duration based on numFrames so it is normalized to a full burst.
        double durationScaler = static_cast<double>(mFramesPerBurst) / numFrames;
        // Skip this callback if numFrames is very small.
        // This can happen when buffers wrap around, particularly when doing sample rate conversion.
        if (durationScaler < 2.0) {
            mAdpfWrapper.onEndCallback(durationScaler);
        }
    }
}

void AudioStreamAAudio::logUnsupportedAttributes() {
    int sdkVersion = getSdkVersion();

    // These attributes are not supported pre Android "P"
    if (sdkVersion < __ANDROID_API_P__) {
        if (mUsage != Usage::Media) {
            LOGW("Usage [AudioStreamBuilder::setUsage()] "
                 "is not supported on AAudio streams running on pre-Android P versions.");
        }

        if (mContentType != ContentType::Music) {
            LOGW("ContentType [AudioStreamBuilder::setContentType()] "
                 "is not supported on AAudio streams running on pre-Android P versions.");
        }

        if (mSessionId != SessionId::None) {
            LOGW("SessionId [AudioStreamBuilder::setSessionId()] "
                 "is not supported on AAudio streams running on pre-Android P versions.");
        }
    }
}

Result AudioStreamAAudio::open() {
    Result result = Result::OK;

    if (mAAudioStream != nullptr) {
        return Result::ErrorInvalidState;
    }

    result = AudioStream::open();
    if (result != Result::OK) {
        return result;
    }

    AAudioStreamBuilder *aaudioBuilder;
    result = static_cast<Result>(mLibLoader->createStreamBuilder(&aaudioBuilder));
    if (result != Result::OK) {
        return result;
    }

    // Do not set INPUT capacity below 4096 because that prevents us from getting a FAST track
    // when using the Legacy data path.
    // If the app requests > 4096 then we allow it but we are less likely to get LowLatency.
    // See internal bug b/80308183 for more details.
    // Fixed in Q but let's still clip the capacity because high input capacity
    // does not increase latency.
    int32_t capacity = mBufferCapacityInFrames;
    constexpr int kCapacityRequiredForFastLegacyTrack = 4096; // matches value in AudioFinger
    if (OboeGlobals::areWorkaroundsEnabled()
            && mDirection == oboe::Direction::Input
            && capacity != oboe::Unspecified
            && capacity < kCapacityRequiredForFastLegacyTrack
            && mPerformanceMode == oboe::PerformanceMode::LowLatency) {
        capacity = kCapacityRequiredForFastLegacyTrack;
        LOGD("AudioStreamAAudio.open() capacity changed from %d to %d for lower latency",
             static_cast<int>(mBufferCapacityInFrames), capacity);
    }
    mLibLoader->builder_setBufferCapacityInFrames(aaudioBuilder, capacity);

    if (mLibLoader->builder_setSessionId != nullptr) {
        mLibLoader->builder_setSessionId(aaudioBuilder,
                                         static_cast<aaudio_session_id_t>(mSessionId));
        // Output effects do not support PerformanceMode::LowLatency.
        if (OboeGlobals::areWorkaroundsEnabled()
                && mSessionId != SessionId::None
                && mDirection == oboe::Direction::Output
                && mPerformanceMode == PerformanceMode::LowLatency) {
                    mPerformanceMode = PerformanceMode::None;
                    LOGD("AudioStreamAAudio.open() performance mode changed to None when session "
                         "id is requested");
        }
    }

    // Channel mask was added in SC_V2. Given the corresponding channel count of selected channel
    // mask may be different from selected channel count, the last set value will be respected.
    // If channel count is set after channel mask, the previously set channel mask will be cleared.
    // If channel mask is set after channel count, the channel count will be automatically
    // calculated from selected channel mask. In that case, only set channel mask when the API
    // is available and the channel mask is specified.
    if (mLibLoader->builder_setChannelMask != nullptr && mChannelMask != ChannelMask::Unspecified) {
        mLibLoader->builder_setChannelMask(aaudioBuilder,
                                           static_cast<aaudio_channel_mask_t>(mChannelMask));
    } else {
        mLibLoader->builder_setChannelCount(aaudioBuilder, mChannelCount);
    }
    mLibLoader->builder_setDeviceId(aaudioBuilder, AudioStreamBase::getDeviceId());
    mLibLoader->builder_setDirection(aaudioBuilder, static_cast<aaudio_direction_t>(mDirection));
    mLibLoader->builder_setFormat(aaudioBuilder, static_cast<aaudio_format_t>(mFormat));
    mLibLoader->builder_setSampleRate(aaudioBuilder, mSampleRate);
    mLibLoader->builder_setSharingMode(aaudioBuilder,
                                       static_cast<aaudio_sharing_mode_t>(mSharingMode));
    mLibLoader->builder_setPerformanceMode(aaudioBuilder,
                                           static_cast<aaudio_performance_mode_t>(mPerformanceMode));

    // These were added in P so we have to check for the function pointer.
    if (mLibLoader->builder_setUsage != nullptr) {
        mLibLoader->builder_setUsage(aaudioBuilder,
                                     static_cast<aaudio_usage_t>(mUsage));
    }

    if (mLibLoader->builder_setContentType != nullptr) {
        mLibLoader->builder_setContentType(aaudioBuilder,
                                           static_cast<aaudio_content_type_t>(mContentType));
    }

    if (mLibLoader->builder_setInputPreset != nullptr) {
        aaudio_input_preset_t inputPreset = mInputPreset;
        if (getSdkVersion() <= __ANDROID_API_P__ && inputPreset == InputPreset::VoicePerformance) {
            LOGD("InputPreset::VoicePerformance not supported before Q. Using VoiceRecognition.");
            inputPreset = InputPreset::VoiceRecognition; // most similar preset
        }
        mLibLoader->builder_setInputPreset(aaudioBuilder,
                                           static_cast<aaudio_input_preset_t>(inputPreset));
    }

    // These were added in S so we have to check for the function pointer.
    if (mLibLoader->builder_setPackageName != nullptr && !mPackageName.empty()) {
        mLibLoader->builder_setPackageName(aaudioBuilder,
                                           mPackageName.c_str());
    }

    if (mLibLoader->builder_setAttributionTag != nullptr && !mAttributionTag.empty()) {
        mLibLoader->builder_setAttributionTag(aaudioBuilder,
                                           mAttributionTag.c_str());
    }

    // This was added in Q so we have to check for the function pointer.
    if (mLibLoader->builder_setAllowedCapturePolicy != nullptr && mDirection == oboe::Direction::Output) {
        mLibLoader->builder_setAllowedCapturePolicy(aaudioBuilder,
                                           static_cast<aaudio_allowed_capture_policy_t>(mAllowedCapturePolicy));
    }

    if (mLibLoader->builder_setPrivacySensitive != nullptr && mDirection == oboe::Direction::Input
            && mPrivacySensitiveMode != PrivacySensitiveMode::Unspecified) {
        mLibLoader->builder_setPrivacySensitive(aaudioBuilder,
                mPrivacySensitiveMode == PrivacySensitiveMode::Enabled);
    }

    if (mLibLoader->builder_setIsContentSpatialized != nullptr) {
        mLibLoader->builder_setIsContentSpatialized(aaudioBuilder, mIsContentSpatialized);
    }

    if (mSpatializationBehavior == SpatializationBehavior::Unspecified) {
        mSpatializationBehavior = SpatializationBehavior::Auto;
    }
    if (mLibLoader->builder_setSpatializationBehavior != nullptr) {
        mLibLoader->builder_setSpatializationBehavior(aaudioBuilder,
                static_cast<aaudio_spatialization_behavior_t>(mSpatializationBehavior));
    } else {
        mSpatializationBehavior = SpatializationBehavior::Auto;
    }

    if (anyDataCallbackSpecified()) {
        if (isDataCallbackSpecified()) {
            if (!acquireDataCallbackHandle()) {
                mLibLoader->builder_delete(aaudioBuilder);
                return Result::ErrorNoFreeHandles;
            }
            mLibLoader->builder_setDataCallback(
                    aaudioBuilder, oboe_aaudio_data_callback_proc,
                    reinterpret_cast<void *>(mDataCallbackHandle));
        } else if (isPartialDataCallbackSpecified()) {
            if (mLibLoader->builder_setPartialDataCallback == nullptr) {
                // This must not happen. The stream should fail open from the builder.
                // But having a check here to avoid crashing.
                LOGE("Using partial data callback while it is not available");
                return Result::ErrorIllegalArgument;
            }
            if (!acquireDataCallbackHandle()) {
                mLibLoader->builder_delete(aaudioBuilder);
                return Result::ErrorNoFreeHandles;
            }
            mLibLoader->builder_setPartialDataCallback(
                    aaudioBuilder, oboe_aaudio_partial_data_callback_proc,
                    reinterpret_cast<void *>(mDataCallbackHandle));
        }
        mLibLoader->builder_setFramesPerDataCallback(aaudioBuilder, getFramesPerDataCallback());

        if (!isErrorCallbackSpecified()) {
            // The app did not specify a callback so we should specify
            // our own so the stream gets closed and stopped.
            mErrorCallback = &mDefaultErrorCallback;
        }
        mLibLoader->builder_setErrorCallback(aaudioBuilder, internalErrorCallback, this);
    }
    // Else if the data callback is not being used then the write method will return an error
    // and the app can stop and close the stream.

    if (isPresentationCallbackSpecified() &&
        mLibLoader->builder_setPresentationEndCallback != nullptr) {
        mLibLoader->builder_setPresentationEndCallback(aaudioBuilder,
                                                       internalPresentationEndCallback,
                                                       this);
    }

    if (mLibLoader->builder_setRoutingChangedCallback != nullptr) {
        mLibLoader->builder_setRoutingChangedCallback(aaudioBuilder,
                                                      internalRoutingChangedCallback,
                                                      this);
    }

    // ============= OPEN THE STREAM ================
    {
        AAudioStream *stream = nullptr;
        result = static_cast<Result>(mLibLoader->builder_openStream(aaudioBuilder, &stream));
        mAAudioStream.store(stream);
    }
    if (result != Result::OK) {
        // Warn developer because ErrorInternal is not very informative.
        if (result == Result::ErrorInternal && mDirection == Direction::Input) {
            LOGW("AudioStreamAAudio.open() may have failed due to lack of "
                 "audio recording permission.");
        }
        goto error2;
    }

    // Query and cache the stream properties
    mChannelCount = mLibLoader->stream_getChannelCount(mAAudioStream);
    mSampleRate = mLibLoader->stream_getSampleRate(mAAudioStream);
    mFormat = static_cast<AudioFormat>(mLibLoader->stream_getFormat(mAAudioStream));
    mSharingMode = static_cast<SharingMode>(mLibLoader->stream_getSharingMode(mAAudioStream));
    mPerformanceMode = static_cast<PerformanceMode>(
            mLibLoader->stream_getPerformanceMode(mAAudioStream));
    mBufferCapacityInFrames = mLibLoader->stream_getBufferCapacity(mAAudioStream);
    mBufferSizeInFrames = mLibLoader->stream_getBufferSize(mAAudioStream);
    mFramesPerBurst = mLibLoader->stream_getFramesPerBurst(mAAudioStream);

    // These were added in P so we have to check for the function pointer.
    if (mLibLoader->stream_getUsage != nullptr) {
        mUsage = static_cast<Usage>(mLibLoader->stream_getUsage(mAAudioStream));
    }
    if (mLibLoader->stream_getContentType != nullptr) {
        mContentType = static_cast<ContentType>(mLibLoader->stream_getContentType(mAAudioStream));
    }
    if (mLibLoader->stream_getInputPreset != nullptr) {
        mInputPreset = static_cast<InputPreset>(mLibLoader->stream_getInputPreset(mAAudioStream));
    }
    if (mLibLoader->stream_getSessionId != nullptr) {
        mSessionId = static_cast<SessionId>(mLibLoader->stream_getSessionId(mAAudioStream));
    } else {
        mSessionId = SessionId::None;
    }

    // This was added in Q so we have to check for the function pointer.
    if (mLibLoader->stream_getAllowedCapturePolicy != nullptr && mDirection == oboe::Direction::Output) {
        mAllowedCapturePolicy = static_cast<AllowedCapturePolicy>(mLibLoader->stream_getAllowedCapturePolicy(mAAudioStream));
    } else {
        mAllowedCapturePolicy = AllowedCapturePolicy::Unspecified;
    }

    if (mLibLoader->stream_isPrivacySensitive != nullptr && mDirection == oboe::Direction::Input) {
        bool isPrivacySensitive = mLibLoader->stream_isPrivacySensitive(mAAudioStream);
        mPrivacySensitiveMode = isPrivacySensitive ? PrivacySensitiveMode::Enabled :
                PrivacySensitiveMode::Disabled;
    } else {
        mPrivacySensitiveMode = PrivacySensitiveMode::Unspecified;
    }

    if (mLibLoader->stream_getChannelMask != nullptr) {
        mChannelMask = static_cast<ChannelMask>(mLibLoader->stream_getChannelMask(mAAudioStream));
    }

    if (mLibLoader->stream_isContentSpatialized != nullptr) {
        mIsContentSpatialized = mLibLoader->stream_isContentSpatialized(mAAudioStream);
    }

    if (mLibLoader->stream_getSpatializationBehavior != nullptr) {
        mSpatializationBehavior = static_cast<SpatializationBehavior>(
                mLibLoader->stream_getSpatializationBehavior(mAAudioStream));
    }

    if (mLibLoader->stream_getHardwareChannelCount != nullptr) {
        mHardwareChannelCount = mLibLoader->stream_getHardwareChannelCount(mAAudioStream);
    }
    if (mLibLoader->stream_getHardwareSampleRate != nullptr) {
        mHardwareSampleRate = mLibLoader->stream_getHardwareSampleRate(mAAudioStream);
    }
    if (mLibLoader->stream_getHardwareFormat != nullptr) {
        mHardwareFormat = static_cast<AudioFormat>(mLibLoader->stream_getHardwareFormat(mAAudioStream));
    }

    updateDeviceIds();

    LOGD("AudioStreamAAudio.open() format=%d, sampleRate=%d, capacity = %d",
            static_cast<int>(mFormat), static_cast<int>(mSampleRate),
            static_cast<int>(mBufferCapacityInFrames));

    calculateDefaultDelayBeforeCloseMillis();

error2:
    mLibLoader->builder_delete(aaudioBuilder);
    if (static_cast<int>(result) > 0) {
        // Possibly due to b/267531411
        LOGW("AudioStreamAAudio.open: AAudioStream_Open() returned positive error = %d",
             static_cast<int>(result));
        if (OboeGlobals::areWorkaroundsEnabled()) {
            result = Result::ErrorInternal; // Coerce to negative error.
        }
    } else {
        LOGD("AudioStreamAAudio.open: AAudioStream_Open() returned %s = %d",
             mLibLoader->convertResultToText(static_cast<aaudio_result_t>(result)),
             static_cast<int>(result));
        if (result == Result::OK) {
            // Only add the stream to collection when successfully open.
            AAudioStreamCollection::getInstance().addStream(
                    this, mAAudioStream.load(), mWeakThis, mWeakThisSet,
                    mWeakParentStream, mWeakParentSet);
        }
    }
    if (result != Result::OK && mHasDataCallbackHandle) {
        // The stream never started, so no callback can be running. Any later one finds a stale
        // handle and returns Stop.
        DataCallbackSlots::release(mDataCallbackHandle, /*waitForCallbacks=*/false);
        mHasDataCallbackHandle = false;
    }
    return result;
}

bool AudioStreamAAudio::acquireDataCallbackHandle() {
    if (!DataCallbackSlots::acquire(this, &mDataCallbackHandle)) {
        LOGE("AudioStreamAAudio.open() too many streams open at once");
        return false;
    }
    mHasDataCallbackHandle = true;
    return true;
}

Result AudioStreamAAudio::release() {
    if (getSdkVersion() < __ANDROID_API_R__) {
        return Result::ErrorUnimplemented;
    }

    // AAudioStream_release() is buggy on Android R.
    if (OboeGlobals::areWorkaroundsEnabled() && getSdkVersion() == __ANDROID_API_R__) {
        LOGW("Skipping release() on Android R");
        return Result::ErrorUnimplemented;
    }

    // Not allowed from the stream's own data callback (see close()). Reject it, but make the
    // callback return DataCallbackResult::Stop so the audio still stops.
    if (isOnOwnDataCallbackThread()) {
        mStopRequestedFromCallback.store(true, std::memory_order_relaxed);
        LOGE("%s() called from the stream's own data callback; rejected. Return "
             "DataCallbackResult::Stop and release from another thread.", __func__);
        return Result::ErrorInvalidState;
    }

    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        if (OboeGlobals::areWorkaroundsEnabled()) {
            // Make sure we are really stopped. Do it under mLock
            // so another thread cannot call requestStart() right before the close.
            requestStop_l(stream);
        }
        return static_cast<Result>(mLibLoader->stream_release(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::close() {
    LOGD("%s", __func__);
    // Closing a stream from its own data callback is not allowed: AAudio rejects it, and it
    // can deadlock on older Android versions. Reject it without closing or unregistering
    // anything, but make the callback return DataCallbackResult::Stop so the audio stops.
    if (isOnOwnDataCallbackThread()) {
        mStopRequestedFromCallback.store(true, std::memory_order_relaxed);
        LOGE("%s() called from the stream's own data callback; rejected. Return "
             "DataCallbackResult::Stop and close from another thread.", __func__);
        return Result::ErrorInvalidState;
    }
    return closeInternal(CloseMode::App);
}

Result AudioStreamAAudio::closeInternal(CloseMode mode) {
    // Always remove the stream from the collection before closing it as after closing, the client
    // will free the resource of the stream.
    AAudioStreamCollection::getInstance().removeStream(this);

    // Serializes close() calls for the whole close, including the wait for data callbacks below,
    // so a second closer (for example the error thread) cannot return early and let the app
    // delete the stream while this close() is still finishing. This stream's own data callback
    // never takes it (close() from there is rejected).
    std::lock_guard<std::mutex> closeLock(mCloseLock);

    AAudioStream *stream = nullptr;
    uintptr_t callbackHandle = 0;
    bool hasCallbackHandle = false;
    {
        // Wait for stream methods that are running (they hold mLock or mAAudioStreamLock), then
        // detach the AAudioStream. After this block no other method can reach it: they all see
        // mAAudioStream == nullptr and return ErrorClosed. So the rest of close() does not need
        // mLock, and a data callback that calls a locking stream method while
        // AAudioStream_close() waits for it does not deadlock.
        std::lock_guard<std::mutex> lock(mLock);
        AudioStream::close();
        std::unique_lock<std::shared_mutex> lock2(mAAudioStreamLock);
        stream = mAAudioStream.exchange(nullptr);
        if (stream != nullptr) {
            callbackHandle = mDataCallbackHandle;
            hasCallbackHandle = mHasDataCallbackHandle;
            mHasDataCallbackHandle = false;
        }
    }
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    // Always stop before closing. On Android Q, closing a running Legacy stream can block
    // forever in ~AudioTrack(), and the destructor and exit handler may close running streams.
    // No other thread can restart the stream now.
    const auto stateBeforeStop = static_cast<StreamState>(mLibLoader->stream_getState(stream));
    requestStopChecked(mLibLoader, stream);
    if (OboeGlobals::areWorkaroundsEnabled() && needsDelayBeforeClose(mode, stateBeforeStop)) {
        sleepBeforeClose();
    }
    const Result result = static_cast<Result>(mLibLoader->stream_close(stream));
    // A Legacy stream's data callback can still be running, or arrive later. Stop it from
    // reaching this object.
    if (hasCallbackHandle) {
        DataCallbackSlots::release(callbackHandle, /*waitForCallbacks=*/true);
    }
    // A data callback that ran after AudioStream::close() above may have re-opened the ADPF
    // session. No callback can reach this stream now, so close it again to avoid a leak.
    closePerformanceHint();
    return result;
}

bool AudioStreamAAudio::needsDelayBeforeClose(CloseMode mode, StreamState stateBeforeStop) {
    switch (mode) {
        case CloseMode::App:
            return true;
        case CloseMode::Destructor:
            // "Stop, then release" is the common correct pattern. If the stream was never
            // started or had already stopped, the delay would only block the releasing thread.
            return !(stateBeforeStop == StreamState::Open
                    || stateBeforeStop == StreamState::Stopped
                    || stateBeforeStop == StreamState::Paused
                    || stateBeforeStop == StreamState::Flushed);
        case CloseMode::Exit:
            return false;  // The process is going away; the extra margin buys nothing.
    }
    return true;
}

// Stops the stream on its own thread (see launchStopThread()). The shared_ptrs, if any, keep the
// stream and its parent alive; the HelperLease keeps a raw-pointer stream from being freed.
static void oboe_stop_thread_proc(AudioStreamAAudio *oboeStream,
                                  uint32_t startGeneration,
                                  std::shared_ptr<AudioStream> sharedStream,
                                  std::shared_ptr<AudioStream> sharedParentStream,
                                  HelperLease lease) {
    lease.attachToCurrentThread();
    (void) sharedStream;
    (void) sharedParentStream;
    oboeStream->requestStopFromStopThread(startGeneration);
    lease.reset();
}

Result AudioStreamAAudio::requestStopFromStopThread(uint32_t startGeneration) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mStartGeneration.load(std::memory_order_relaxed) != startGeneration) {
        // The app called requestStart() after the callback asked to stop. The later call wins,
        // as it would if the callback had stopped the stream synchronously.
        return Result::OK;
    }
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    return requestStop_l(stream);
}

// Called on the data callback thread, so this must not take the registry lock. The stream
// and any parent stay alive while the data callback runs (unless the app releases its last
// reference inside onAudioReady(), which is not supported). mWeakThis, mWeakThisSet,
// mWeakParentStream and mWeakParentSet are only written during openStream(), before start.
// This path (workarounds enabled, Android R and older) already spawns a thread. Taking the
// HelperLease is a lock-free atomic increment.
void AudioStreamAAudio::launchStopThread() {
    // Prevent multiple stop threads from being launched.
    if (mStopThreadAllowed.exchange(false)) {
        std::shared_ptr<AudioStream> sharedStream = lockWeakThis();   // atomic refcount, no lock
        std::shared_ptr<AudioStream> sharedParentStream =
                mWeakParentSet ? mWeakParentStream.lock() : nullptr;
        if ((mWeakThisSet && !sharedStream) || (mWeakParentSet && !sharedParentStream)) {
            // The app already released the stream; its teardown will stop and close it.
            return;
        }
        // Stop this stream on a separate thread. Move the refs so this callback never
        // drops the last one.
        const uint32_t startGeneration = mStartGeneration.load(std::memory_order_relaxed);
        std::thread t(oboe_stop_thread_proc, this, startGeneration, std::move(sharedStream),
                      std::move(sharedParentStream), HelperLease(mHelperThreads));
        t.detach();
    }
}

DataCallbackResult AudioStreamAAudio::callOnAudioReady(AAudioStream * /*stream*/,
                                                       void *audioData,
                                                       int32_t numFrames) {
    // Lock-free marker so requestStop()/stop()/close()/release() and the destructor can tell
    // they were called from inside this callback (see isOnOwnDataCallbackThread()).
    mDataCallbackThread.store(static_cast<uintptr_t>(pthread_self()), std::memory_order_relaxed);
    DataCallbackResult result = fireDataCallback(audioData, numFrames);
    if (mStopRequestedFromCallback.load(std::memory_order_relaxed)) {
        // The app called requestStop(), stop(), close() or release() from this callback.
        // Those calls only set the flag, so stop the stream here by returning Stop. Also stop
        // calling the app: on Android R and older, Stop is turned into Continue below until the
        // stop thread has stopped the stream. requestStart() re-enables the callback. (If
        // another thread calls requestStart() at this exact moment, the restart can be lost;
        // the same is true when the app itself returns Stop.)
        setDataCallbackEnabled(false);
        result = DataCallbackResult::Stop;
    }
    if (result != DataCallbackResult::Continue) {
        if (result == DataCallbackResult::Stop) {
            LOGD("Oboe callback returned DataCallbackResult::Stop");
        } else {
            LOGE("Oboe callback returned unexpected value. Error: %d", static_cast<int>(result));
        }

        // Returning Stop caused various problems before S. See #1230
        if (OboeGlobals::areWorkaroundsEnabled() && getSdkVersion() <= __ANDROID_API_R__) {
            launchStopThread();
            result = DataCallbackResult::Continue;
        } else {
            result = DataCallbackResult::Stop; // OK >= API_S
        }
    }
    mDataCallbackThread.store(0, std::memory_order_relaxed);
    return result;
}

int32_t AudioStreamAAudio::callOnPartialAudioReady(AAudioStream * /*stream*/,
                                                   void *audioData,
                                                   int32_t numFrames) {
    mDataCallbackThread.store(static_cast<uintptr_t>(pthread_self()), std::memory_order_relaxed);
    int32_t result = firePartialDataCallback(audioData, numFrames);
    if (mStopRequestedFromCallback.load(std::memory_order_relaxed)) {
        // The app requested a stop from this callback (see callOnAudioReady()).
        result = -1; // A negative result stops the stream.
    }
    mDataCallbackThread.store(0, std::memory_order_relaxed);
    return result;
}

bool AudioStreamAAudio::isControlCallFromOwnCallbackRejected() const {
    return isOnOwnDataCallbackThread() && getSdkVersion() >= __ANDROID_API_P__;
}

Result AudioStreamAAudio::requestStart() {
    if (isControlCallFromOwnCallbackRejected()) {
        return Result::ErrorInvalidState;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        // Cancel any stop that the data callback requested but that has not happened yet
        // (see requestStopFromStopThread()), and re-enable the app's callback. Done before the
        // O_MR1 early return below, so a start always wins over an earlier pending stop.
        mStartGeneration.fetch_add(1, std::memory_order_relaxed);
        if (anyDataCallbackSpecified()) {
            setDataCallbackEnabled(true);
        }
        mStopThreadAllowed = true;
        mStopRequestedFromCallback.store(false, std::memory_order_relaxed);
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(stream));
            if (state == StreamState::Starting || state == StreamState::Started) {
                // WARNING: On P, AAudio is returning ErrorInvalidState for Output and OK for Input.
                return Result::OK;
            }
        }
        closePerformanceHint();
        return static_cast<Result>(mLibLoader->stream_requestStart(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestPause() {
    if (isControlCallFromOwnCallbackRejected()) {
        return Result::ErrorInvalidState;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(stream));
            if (state == StreamState::Pausing || state == StreamState::Paused) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestPause(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestFlush() {
    if (isControlCallFromOwnCallbackRejected()) {
        return Result::ErrorInvalidState;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(stream));
            if (state == StreamState::Flushing || state == StreamState::Flushed) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestFlush(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestStop() {
    if (isOnOwnDataCallbackThread()) {
        // Called from this stream's own data callback. Keep the real-time thread lock-free:
        // make the callback return DataCallbackResult::Stop instead of taking mLock.
        // Returns OK immediately; the state changes only after the callback returns.
        mStopRequestedFromCallback.store(true, std::memory_order_relaxed);
        return Result::OK;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return requestStop_l(stream);
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::stop(int64_t timeoutNanoseconds) {
    if (isOnOwnDataCallbackThread()) {
        // Called from this stream's own data callback. The stream cannot reach Stopped until
        // the callback returns, so waiting would only stall the real-time thread until the
        // timeout. Request the stop and return OK; the state changes after the callback returns.
        return requestStop();
    }
    return AudioStream::stop(timeoutNanoseconds);
}

// Call under mLock
Result AudioStreamAAudio::requestStop_l(AAudioStream *stream) {
    return requestStopChecked(mLibLoader, stream);
}

ResultWithValue<int32_t>   AudioStreamAAudio::write(const void *buffer,
                                     int32_t numFrames,
                                     int64_t timeoutNanoseconds) {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t result = mLibLoader->stream_write(mAAudioStream, buffer,
                                                  numFrames, timeoutNanoseconds);
        return ResultWithValue<int32_t>::createBasedOnSign(result);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

ResultWithValue<int32_t>   AudioStreamAAudio::read(void *buffer,
                                 int32_t numFrames,
                                 int64_t timeoutNanoseconds) {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t result = mLibLoader->stream_read(mAAudioStream, buffer,
                                                 numFrames, timeoutNanoseconds);
        return ResultWithValue<int32_t>::createBasedOnSign(result);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}


// AAudioStream_waitForStateChange() can crash if it is waiting on a stream and that stream
// is closed from another thread.  We do not want to lock the stream for the duration of the call.
// So we call AAudioStream_waitForStateChange() with a timeout of zero so that it will not block.
// Then we can do our own sleep with the lock unlocked.
Result AudioStreamAAudio::waitForStateChange(StreamState currentState,
                                        StreamState *nextState,
                                        int64_t timeoutNanoseconds) {
    Result oboeResult = Result::ErrorTimeout;
    int64_t sleepTimeNanos = 20 * kNanosPerMillisecond; // arbitrary
    aaudio_stream_state_t currentAAudioState = static_cast<aaudio_stream_state_t>(currentState);

    aaudio_result_t result = AAUDIO_OK;
    int64_t timeLeftNanos = timeoutNanoseconds;

    mLock.lock();
    while (true) {
        // Do we still have an AAudio stream? If not then stream must have been closed.
        AAudioStream *stream = mAAudioStream.load();
        if (stream == nullptr) {
            if (nextState != nullptr) {
                *nextState = StreamState::Closed;
            }
            oboeResult = Result::ErrorClosed;
            break;
        }

        // Update and query state change with no blocking.
        aaudio_stream_state_t aaudioNextState;
        result = mLibLoader->stream_waitForStateChange(
                mAAudioStream,
                currentAAudioState,
                &aaudioNextState,
                0); // timeout=0 for non-blocking
        // AAudio will return AAUDIO_ERROR_TIMEOUT if timeout=0 and the state does not change.
        if (result != AAUDIO_OK && result != AAUDIO_ERROR_TIMEOUT) {
            oboeResult = static_cast<Result>(result);
            break;
        }
#if OBOE_FIX_FORCE_STARTING_TO_STARTED
        if (OboeGlobals::areWorkaroundsEnabled()
            && aaudioNextState == static_cast<aaudio_stream_state_t >(StreamState::Starting)) {
            aaudioNextState = static_cast<aaudio_stream_state_t >(StreamState::Started);
        }
#endif // OBOE_FIX_FORCE_STARTING_TO_STARTED
        if (nextState != nullptr) {
            *nextState = static_cast<StreamState>(aaudioNextState);
        }
        if (currentAAudioState != aaudioNextState) { // state changed?
            oboeResult = Result::OK;
            break;
        }

        // Did we timeout or did user ask for non-blocking?
        if (timeLeftNanos <= 0) {
            break;
        }

        // No change yet so sleep.
        mLock.unlock(); // Don't sleep while locked.
        if (sleepTimeNanos > timeLeftNanos) {
            sleepTimeNanos = timeLeftNanos; // last little bit
        }
        AudioClock::sleepForNanos(sleepTimeNanos);
        timeLeftNanos -= sleepTimeNanos;
        mLock.lock();
    }

    mLock.unlock();
    return oboeResult;
}

ResultWithValue<int32_t> AudioStreamAAudio::setBufferSizeInFrames(int32_t requestedFrames) {
    int32_t adjustedFrames = requestedFrames;
    if (adjustedFrames > mBufferCapacityInFrames) {
        adjustedFrames = mBufferCapacityInFrames;
    }
    // This calls getBufferSize() so avoid recursive lock.
    adjustedFrames = QuirksManager::getInstance().clipBufferSize(*this, adjustedFrames);

    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t newBufferSize = mLibLoader->stream_setBufferSize(mAAudioStream, adjustedFrames);
        // Cache the result if it's valid
        if (newBufferSize > 0) mBufferSizeInFrames = newBufferSize;
        return ResultWithValue<int32_t>::createBasedOnSign(newBufferSize);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

StreamState AudioStreamAAudio::getState() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        aaudio_stream_state_t aaudioState = mLibLoader->stream_getState(stream);
#if OBOE_FIX_FORCE_STARTING_TO_STARTED
        if (OboeGlobals::areWorkaroundsEnabled()
            && aaudioState == AAUDIO_STREAM_STATE_STARTING) {
            aaudioState = AAUDIO_STREAM_STATE_STARTED;
        }
#endif // OBOE_FIX_FORCE_STARTING_TO_STARTED
        return static_cast<StreamState>(aaudioState);
    } else {
        return StreamState::Closed;
    }
}

void AudioStreamAAudio::onRoutingChanged(std::vector<int32_t> deviceIds) {
    std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
    mUpdatedDeviceIds.deviceIds = std::move(deviceIds);
}

int32_t AudioStreamAAudio::getDeviceId() const {
    std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
    return mUpdatedDeviceIds.deviceIds.empty() ? kUnspecified : mUpdatedDeviceIds.deviceIds[0];
}

std::vector<int32_t> AudioStreamAAudio::getDeviceIds() const {
    std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
    return mUpdatedDeviceIds.deviceIds;
}

int32_t AudioStreamAAudio::getBufferSizeInFrames() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        mBufferSizeInFrames = mLibLoader->stream_getBufferSize(stream);
    }
    return mBufferSizeInFrames;
}

void AudioStreamAAudio::updateFramesRead() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
// Set to 1 for debugging race condition #1180 with mAAudioStream.
// See also DEBUG_CLOSE_RACE in OboeTester.
// This was left in the code so that we could test the fix again easily in the future.
// We could not trigger the race condition without adding these get calls and the sleeps.
#define DEBUG_CLOSE_RACE 0
#if DEBUG_CLOSE_RACE
    // This is used when testing race conditions with close().
    // See DEBUG_CLOSE_RACE in OboeTester
    AudioClock::sleepForNanos(400 * kNanosPerMillisecond);
#endif // DEBUG_CLOSE_RACE
    if (stream != nullptr) {
        mFramesRead = mLibLoader->stream_getFramesRead(stream);
    }
}

void AudioStreamAAudio::updateFramesWritten() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        mFramesWritten = mLibLoader->stream_getFramesWritten(stream);
    }
}

ResultWithValue<int32_t> AudioStreamAAudio::getXRunCount() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return ResultWithValue<int32_t>::createBasedOnSign(mLibLoader->stream_getXRunCount(stream));
    } else {
        return ResultWithValue<int32_t>(Result::ErrorNull);
    }
}

Result AudioStreamAAudio::getTimestamp(clockid_t clockId,
                                   int64_t *framePosition,
                                   int64_t *timeNanoseconds) {
    if (getState() != StreamState::Started) {
        return Result::ErrorInvalidState;
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_getTimestamp(stream, clockId,
                                               framePosition, timeNanoseconds));
    } else {
        return Result::ErrorNull;
    }
}

ResultWithValue<double> AudioStreamAAudio::calculateLatencyMillis() {
    // Get the time that a known audio frame was presented.
    int64_t hardwareFrameIndex;
    int64_t hardwareFrameHardwareTime;
    auto result = getTimestamp(CLOCK_MONOTONIC,
                               &hardwareFrameIndex,
                               &hardwareFrameHardwareTime);
    if (result != oboe::Result::OK) {
        return ResultWithValue<double>(static_cast<Result>(result));
    }

    // Get counter closest to the app.
    bool isOutput = (getDirection() == oboe::Direction::Output);
    int64_t appFrameIndex = isOutput ? getFramesWritten() : getFramesRead();

    // Assume that the next frame will be processed at the current time
    using namespace std::chrono;
    int64_t appFrameAppTime =
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();

    // Calculate the number of frames between app and hardware
    int64_t frameIndexDelta = appFrameIndex - hardwareFrameIndex;

    // Calculate the time which the next frame will be or was presented
    int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) / getSampleRate();
    int64_t appFrameHardwareTime = hardwareFrameHardwareTime + frameTimeDelta;

    // The current latency is the difference in time between when the current frame is at
    // the app and when it is at the hardware.
    double latencyNanos = static_cast<double>(isOutput
                          ? (appFrameHardwareTime - appFrameAppTime) // hardware is later
                          : (appFrameAppTime - appFrameHardwareTime)); // hardware is earlier
    double latencyMillis = latencyNanos / kNanosPerMillisecond;

    return ResultWithValue<double>(latencyMillis);
}

bool AudioStreamAAudio::isMMapUsed() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return AAudioExtensions::getInstance().isMMapUsed(stream);
    } else {
        return false;
    }
}

// static
// Static method for the presentation end callback.
// We use a method so we can access protected methods on the stream.
// Launch a thread to handle the error.
// That other thread can safely stop, close and delete the stream.
void AudioStreamAAudio::internalPresentationEndCallback(AAudioStream *stream, void *userData) {
    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(userData);

    // Keeps the stream alive while the callback is handled: a shared_ptr if the app uses
    // AudioStreamBuilder::openStream(shared_ptr), and a HelperLease in every case.
    auto [isStreamAlive, sharedStream, sharedParentStream, lease] =
            AAudioStreamCollection::getInstance().getStream(oboeStream, stream);
    if (!isStreamAlive) {
        // Client has closed the stream, no need to call the presentation end callback here.
        releaseRefsOffCallbackThread(std::move(sharedStream), std::move(sharedParentStream));
        return;
    }

    if (stream != oboeStream->getUnderlyingStream()) {
        LOGW("%s() stream already closed or closing", __func__); // might happen if there are bugs
    } else if (sharedStream) {
        // Handle presentation end on a separate thread using shared pointer. Move the refs so
        // this AAudio thread never drops the last one.
        std::thread t(oboe_aaudio_presentation_end_thread_proc_shared, std::move(sharedStream),
                      std::move(sharedParentStream), std::move(lease));
        t.detach();
    } else {
        // Handle presentation end on a separate thread.
        std::thread t(oboe_aaudio_presentation_thread_proc, oboeStream, std::move(lease));
        t.detach();
    }
    releaseRefsOffCallbackThread(std::move(sharedStream), std::move(sharedParentStream));
}

void AudioStreamAAudio::internalRoutingChangedCallback(
        AAudioStream *stream, void *userData, const int32_t *deviceIds, int32_t numDevices) {
    AudioStreamAAudio *oboeStream = reinterpret_cast<AudioStreamAAudio*>(userData);

    // Keeps the stream alive while the callback is handled: a shared_ptr if the app uses
    // AudioStreamBuilder::openStream(shared_ptr), and a HelperLease in every case.
    auto [isStreamAlive, sharedStream, sharedParentStream, lease] =
            AAudioStreamCollection::getInstance().getStream(oboeStream, stream);
    if (!isStreamAlive) {
        // Client has closed the stream, no need to call the routing changed callback here.
        releaseRefsOffCallbackThread(std::move(sharedStream), std::move(sharedParentStream));
        return;
    }

    std::vector<int32_t> deviceIdsCopy(deviceIds, deviceIds + numDevices);

    if (stream != oboeStream->getUnderlyingStream()) {
        LOGW("%s() stream already closed or closing", __func__); // might happen if there are bugs
    } else if (sharedStream) {
        oboeStream->onRoutingChanged(deviceIdsCopy);
        if (oboeStream->getRoutingCallback() != nullptr) {
            // Handle routing change on a separate thread using shared pointer. Move the refs
            // so this AAudio thread never drops the last one.
            std::thread t(oboe_aaudio_routing_changed_thread_proc_shared, std::move(sharedStream),
                          std::move(sharedParentStream), deviceIdsCopy, std::move(lease));
            t.detach();
        }
    } else {
        oboeStream->onRoutingChanged(deviceIdsCopy);
        if (oboeStream->getRoutingCallback() != nullptr) {
            // Handle routing change on a separate thread.
            std::thread t(oboe_aaudio_routing_changed_thread_proc, oboeStream, deviceIdsCopy,
                          std::move(lease));
            t.detach();
        }
    }
    releaseRefsOffCallbackThread(std::move(sharedStream), std::move(sharedParentStream));
}

Result AudioStreamAAudio::setOffloadDelayPadding(
        int32_t delayInFrames, int32_t paddingInFrames) {
    if (mLibLoader->stream_setOffloadDelayPadding == nullptr) {
        return Result::ErrorUnimplemented;
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    return static_cast<Result>(
            mLibLoader->stream_setOffloadDelayPadding(stream, delayInFrames, paddingInFrames));
}

ResultWithValue<int32_t> AudioStreamAAudio::getOffloadDelay() {
    if (mLibLoader->stream_getOffloadDelay == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorUnimplemented);
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    return ResultWithValue<int32_t>::createBasedOnSign(mLibLoader->stream_getOffloadDelay(stream));
}

ResultWithValue<int32_t> AudioStreamAAudio::getOffloadPadding() {
    if (mLibLoader->stream_getOffloadPadding == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorUnimplemented);
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
    return ResultWithValue<int32_t>::createBasedOnSign(
            mLibLoader->stream_getOffloadPadding(stream));
}

Result AudioStreamAAudio::setOffloadEndOfStream() {
    if (mLibLoader->stream_setOffloadEndOfStream == nullptr) {
        return Result::ErrorUnimplemented;
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
    return static_cast<Result>(mLibLoader->stream_setOffloadEndOfStream(stream));
}

void AudioStreamAAudio::updateDeviceIds() {
    // If stream_getDeviceIds is not supported, use stream_getDeviceId.
    if (mLibLoader->stream_getDeviceIds == nullptr) {
        mDeviceIds.clear();
        int32_t deviceId = mLibLoader->stream_getDeviceId(mAAudioStream);
        if (deviceId != kUnspecified) {
            mDeviceIds.push_back(deviceId);
        }
    } else {
        // Allocate a temp vector with 16 elements. This should be enough to cover all cases.
        // Please file a bug on Oboe if you discover that this returns AAUDIO_ERROR_OUT_OF_RANGE.
        // When AAUDIO_ERROR_OUT_OF_RANGE is returned, the actual size will be still returned as the
        // value of deviceIdSize but deviceIds will be empty.

        static constexpr int kDefaultDeviceIdSize = 16;
        int deviceIdSize = kDefaultDeviceIdSize;
        std::vector<int32_t> deviceIds(deviceIdSize);
        aaudio_result_t getDeviceIdResult =
                mLibLoader->stream_getDeviceIds(mAAudioStream, deviceIds.data(), &deviceIdSize);
        if (getDeviceIdResult != AAUDIO_OK) {
            LOGE("stream_getDeviceIds did not return AAUDIO_OK. Error: %d",
                    static_cast<int>(getDeviceIdResult));
            return;
        }

        mDeviceIds.clear();
        for (int i = 0; i < deviceIdSize; i++) {
            mDeviceIds.push_back(deviceIds[i]);
        }
    }
    {
        std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
        mUpdatedDeviceIds.deviceIds = mDeviceIds;
    }

    // This should not happen in most cases. Please file a bug on Oboe if you see this happening.
    if (getDeviceIds().empty()) {
        LOGW("updateDeviceIds() returns an empty array.");
    }
}

ResultWithValue<int64_t> AudioStreamAAudio::flushFromFrame(FlushFromAccuracy accuracy,
                                                           int64_t positionInFrames) {
    if (mLibLoader->stream_flushFromFrame == nullptr) {
        return ResultWithValue<int64_t>(positionInFrames, Result::ErrorUnimplemented);
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ResultWithValue<int64_t>(positionInFrames, Result::ErrorClosed);
    }
    // TODO: use aaudio_flush_from_frame_accuracy_t when it is defined.
    auto result = static_cast<Result>(mLibLoader->stream_flushFromFrame(
                    stream, static_cast<int32_t>(accuracy), &positionInFrames));
    return ResultWithValue<int64_t>(positionInFrames, result);
}

namespace {

ResultWithValue<AAudioPlaybackParameters> oboe2AAudio_PlaybackParameters_AAudioPlaybackParameters(
        const PlaybackParameters& playbackParameters) {
    AAudioPlaybackParameters aaudioPlaybackParameters;
    switch (playbackParameters.fallbackMode) {
        case FallbackMode::Default:
            aaudioPlaybackParameters.fallbackMode = AAUDIO_FALLBACK_MODE_DEFAULT;
            break;
        case FallbackMode::Mute:
            aaudioPlaybackParameters.fallbackMode = AAUDIO_FALLBACK_MODE_MUTE;
            break;
        case FallbackMode::Fail:
            aaudioPlaybackParameters.fallbackMode = AAUDIO_FALLBACK_MODE_FAIL;
            break;
        default:
            return ResultWithValue<AAudioPlaybackParameters>(Result::ErrorIllegalArgument);
    }

    switch (playbackParameters.stretchMode) {
        case StretchMode::Default:
            aaudioPlaybackParameters.stretchMode = AAUDIO_STRETCH_MODE_DEFAULT;
            break;
        case StretchMode::Voice:
            aaudioPlaybackParameters.stretchMode = AAUDIO_STRETCH_MODE_VOICE;
            break;
        default:
            return ResultWithValue<AAudioPlaybackParameters>(Result::ErrorIllegalArgument);
    }

    aaudioPlaybackParameters.pitch = playbackParameters.pitch;
    aaudioPlaybackParameters.speed = playbackParameters.speed;
    return ResultWithValue<AAudioPlaybackParameters>(aaudioPlaybackParameters);
}

ResultWithValue<PlaybackParameters> aaudio2oboe_AAudioPlaybackParameters_PlaybackParameters(
        const AAudioPlaybackParameters& aaudioPlaybackParameters) {
    PlaybackParameters playbackParameters;
    switch (aaudioPlaybackParameters.fallbackMode) {
        case AAUDIO_FALLBACK_MODE_DEFAULT:
            playbackParameters.fallbackMode = FallbackMode::Default;
            break;
        case AAUDIO_FALLBACK_MODE_MUTE:
            playbackParameters.fallbackMode = FallbackMode::Mute;
            break;
        case AAUDIO_FALLBACK_MODE_FAIL:
            playbackParameters.fallbackMode = FallbackMode::Fail;
            break;
        default:
            LOGE("%s unknown fallback mode %d", __func__, aaudioPlaybackParameters.fallbackMode);
            return ResultWithValue<PlaybackParameters>(Result::ErrorIllegalArgument);
    }

    switch (aaudioPlaybackParameters.stretchMode) {
        case AAUDIO_STRETCH_MODE_DEFAULT:
            playbackParameters.stretchMode = StretchMode::Default;
            break;
        case AAUDIO_STRETCH_MODE_VOICE:
            playbackParameters.stretchMode = StretchMode::Voice;
            break;
        default:
            LOGE("%s unknown stretch mode %d", __func__, aaudioPlaybackParameters.stretchMode);
            return ResultWithValue<PlaybackParameters>(Result::ErrorIllegalArgument);
    }
    playbackParameters.pitch = aaudioPlaybackParameters.pitch;
    playbackParameters.speed = aaudioPlaybackParameters.speed;
    return ResultWithValue<PlaybackParameters>(playbackParameters);
}

} // namespace

Result AudioStreamAAudio::setPlaybackParameters(const PlaybackParameters &parameters) {
    if (mLibLoader->stream_setPlaybackParameters == nullptr) {
        LOGD("%s, the NDK function is not available", __func__);
        return Result::ErrorUnimplemented;
    }
    std::shared_lock _l(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        LOGE("%s the stream is already closed", __func__);
        return Result::ErrorClosed;
    }
    auto convertResult =
            oboe2AAudio_PlaybackParameters_AAudioPlaybackParameters(parameters);
    if (!convertResult) {
        LOGE("%s, invalid parameters, %s", __func__, toString(parameters).c_str());
        return Result::ErrorIllegalArgument;
    }
    auto aaudioPlaybackParameters = convertResult.value();
    return static_cast<Result>(mLibLoader->stream_setPlaybackParameters(
            stream, &aaudioPlaybackParameters));
}

ResultWithValue<PlaybackParameters> AudioStreamAAudio::getPlaybackParameters() {
    if (mLibLoader->stream_getPlaybackParameters == nullptr) {
        LOGD("%s, the NDK function is not available", __func__);
        return Result::ErrorUnimplemented;
    }
    std::shared_lock _l(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        LOGE("%s the stream is already closed", __func__);
        return Result::ErrorClosed;
    }

    AAudioPlaybackParameters aaudioPlaybackParameters;
    auto result = static_cast<Result>(
            mLibLoader->stream_getPlaybackParameters(stream, &aaudioPlaybackParameters));
    if (result != Result::OK) {
        return ResultWithValue<PlaybackParameters>(result);
    }

    return aaudio2oboe_AAudioPlaybackParameters_PlaybackParameters(aaudioPlaybackParameters);
}

} // namespace oboe
