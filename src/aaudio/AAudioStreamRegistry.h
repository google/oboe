/*
 * Copyright 2026 The Android Open Source Project
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

#ifndef OBOE_AAUDIO_STREAM_REGISTRY_H_
#define OBOE_AAUDIO_STREAM_REGISTRY_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "aaudio/AudioStreamAAudio.h"
#include "common/OboeDebug.h"

namespace oboe {

/**
 * Counts Oboe helper threads (error, routing, presentation, stop) using one stream so the
 * destructor can wait for them before freeing memory (protects raw-pointer streams).
 * Lock-free and shared_ptr-owned so it outlives the stream.
 */
class HelperThreadTracker {
public:
    void acquire() { mCount.fetch_add(1, std::memory_order_relaxed); }

    // Release ordering: helper's last use of the stream happens before the destructor frees it.
    void release() { mCount.fetch_sub(1, std::memory_order_release); }

    // Waits until no helper thread other than the caller holds a lease, logging periodically
    // if an app callback appears blocked.
    void waitForOtherThreads(const void *stream) const {
        if (mCount.load(std::memory_order_acquire) == 0) {
            return; // Fast path: avoids touching thread_local on non-helper threads.
        }
        const int own = (attachedTracker() == this) ? 1 : 0;
        constexpr auto kWarnAfter = std::chrono::milliseconds(500);
        constexpr auto kWarnEvery = std::chrono::milliseconds(2000);
        auto nextWarn = std::chrono::steady_clock::now() + kWarnAfter;
        int64_t waitedMillis = kWarnAfter.count();
        while (mCount.load(std::memory_order_acquire) > own) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextWarn) {
                LOGE("AudioStreamAAudio(%p) deleted while an Oboe callback thread still uses it; "
                     "waited %lld ms for the app's error/routing/presentation callback to return. "
                     "Is that callback blocked on the thread deleting the stream?",
                     stream, static_cast<long long>(waitedMillis));
                waitedMillis += kWarnEvery.count();
                nextWarn = now + kWarnEvery;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    static const HelperThreadTracker *&attachedTracker() {
        static thread_local const HelperThreadTracker *tAttached = nullptr;
        return tAttached;
    }

private:
    std::atomic<int> mCount{0};
};

/**
 * Move-only RAII lease on a stream's HelperThreadTracker. Acquired before spawning a helper
 * thread and released after its last use of the stream.
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

    void attachToCurrentThread() {
        if (mTracker && !mAttached) {
            HelperThreadTracker::attachedTracker() = mTracker.get();
            mAttached = true;
        }
    }

    void reset() {
        if (mTracker) {
            if (mAttached) {
                HelperThreadTracker::attachedTracker() = nullptr;
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
 * Maps opaque handle values passed as AAudio callback userData to open AudioStreamAAudio
 * instances, so AAudio never holds a raw pointer to memory that may be freed.
 *
 * Split into two planes sharing one handle (10-bit slot index + non-zero generation):
 * - Hot RT data plane (`sDataSlots`): fixed, trivially destructible static array (24 KB on
 *   64-bit, demand-paged). Data callbacks use enterDataCallback()/leaveDataCallback() —
 *   lock-free, wait-free, allocation-free, and safe during exit().
 * - Cold control plane (`ControlState`): intentionally leaked heap singleton guarded by one
 *   leaf mutex. Used by open/close, exit cleanup, and error/routing/presentation/stop callbacks.
 */
class AAudioStreamRegistry {
public:
    struct DataSlot {
        std::atomic<uint32_t> generation{0};
        std::atomic<AudioStreamAAudio *> stream{nullptr};
        std::atomic<int32_t> active{0}; // data callbacks currently using this slot
    };
    static_assert(std::is_trivially_destructible_v<DataSlot>,
                  "DataSlot must be trivially destructible for exit() safety");

    struct CallbackRefs {
        AudioStreamAAudio *stream = nullptr;
        std::weak_ptr<AudioStream> weakStream;
        std::weak_ptr<AudioStream> weakParentStream;
        bool usesSharedPtr = false;
        bool usesParentSharedPtr = false;
        HelperLease lease;

        explicit operator bool() const { return stream != nullptr; }

        bool lockSharedPtrs(std::shared_ptr<AudioStream> &streamOut,
                            std::shared_ptr<AudioStream> &parentOut) {
            std::shared_ptr<AudioStream> lockedParent;
            std::shared_ptr<AudioStream> lockedStream;
            if (usesParentSharedPtr) {
                lockedParent = weakParentStream.lock();
                if (!lockedParent) {
                    lease.reset();
                    return false;
                }
            }
            if (usesSharedPtr) {
                lockedStream = weakStream.lock();
                if (!lockedStream) {
                    // Release lease before lockedParent is destroyed so we cannot self-deadlock
                    // in waitForOtherThreads() if lockedParent was the last reference.
                    lease.reset();
                    return false;
                }
            }
            streamOut = std::move(lockedStream);
            parentOut = std::move(lockedParent);
            return true;
        }
    };

    // Allocates a free slot for `stream` and outputs a non-zero handle. False if full.
    static bool acquire(AudioStreamAAudio *stream, uintptr_t *handleOut) {
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        for (uint32_t index = 0; index < kMaxSlots; ++index) {
            if (index < state.slots.size() && state.slots[index].inUse) {
                continue;
            }
            DataSlot &data = sDataSlots[index];
            // Skip any slot whose callback is still unwinding after self-destruction.
            if (data.active.load(std::memory_order_acquire) != 0) {
                continue;
            }
            if (index >= state.slots.size()) {
                state.slots.resize(index + 1);
            }
            state.slots[index].inUse = true;
            uint32_t gen = data.generation.load(std::memory_order_relaxed);
            // Never issue generation 0 (including handle 0), on first use or 32-bit wrap.
            if ((gen & kGenerationMask) == 0) {
                gen = data.generation.fetch_add(1, std::memory_order_relaxed) + 1;
            }
            data.stream.store(stream, std::memory_order_release);
            *handleOut = ((static_cast<uintptr_t>(gen) & kGenerationMask) << kIndexBits) | index;
            return true;
        }
        return false;
    }

    // Publishes control-plane metadata after AAudioStreamBuilder_openStream() succeeds.
    static void activate(uintptr_t handle,
                         AudioStreamAAudio *stream,
                         const std::shared_ptr<HelperThreadTracker> &helperThreads,
                         const std::shared_ptr<AudioStream> &selfShared,
                         const std::shared_ptr<AudioStream> &parentShared) {
        const AudioStream *parentStream =
                stream->hasParentStream() ? stream->getParentStream() : nullptr;
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        ControlSlot *slot = findSlotLocked(state, handle);
        if (slot == nullptr) return;
        slot->active = true;
        slot->oboeStream = stream;
        slot->parentStream = parentStream;
        slot->weakStream = selfShared;
        slot->weakParentStream = parentShared;
        slot->usesSharedPtr = (selfShared != nullptr);
        slot->usesParentSharedPtr = (parentShared != nullptr);
        slot->helperThreads = helperThreads;
    }

    // Updates weak_ptr metadata after AudioStreamBuilder wraps a stream (or FilterAudioStream
    // parent) in a shared_ptr. Reads stream->getParentStream() outside the lock.
    static void onWeakThisSet(AudioStream *stream,
                              const std::shared_ptr<AudioStream> &sharedStream) {
        if (stream == nullptr) return;
        const AudioStream *parentStream =
                stream->hasParentStream() ? stream->getParentStream() : nullptr;
        const bool usesShared = (sharedStream != nullptr);
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        for (ControlSlot &slot : state.slots) {
            if (slot.active && static_cast<const AudioStream *>(slot.oboeStream) == stream) {
                slot.parentStream = parentStream;
                slot.weakStream = sharedStream;
                slot.usesSharedPtr = usesShared;
                return;
            }
        }
        for (ControlSlot &slot : state.slots) {
            if (slot.active && slot.parentStream == stream) {
                slot.weakParentStream = sharedStream;
                slot.usesParentSharedPtr = usesShared;
                return;
            }
        }
    }

    // Phase 1 of teardown: blocks new error/routing/presentation/stop callbacks from acquiring
    // leases while keeping the data-callback slot intact until AAudioStream_close() returns.
    static void deactivate(uintptr_t handle) {
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        if (ControlSlot *slot = findSlotLocked(state, handle)) {
            slot->clearControl();
        }
    }

    // Phase 2 of teardown: invalidates the data-callback slot, optionally waits for running
    // data callbacks, and returns the slot to the pool.
    static void release(uintptr_t handle, bool waitForCallbacks) {
        const uintptr_t expectedGen = handle >> kIndexBits;
        if (expectedGen == 0) return;
        const uint32_t index = static_cast<uint32_t>(handle & kIndexMask);
        DataSlot &data = sDataSlots[index];
        uint32_t gen = data.generation.load(std::memory_order_acquire);
        if ((gen & kGenerationMask) != expectedGen) return;
        // seq_cst CAS claims ownership of releasing this generation and pairs with
        // enterDataCallback(): either an entering callback sees the bumped generation, or we see
        // active > 0 and wait for it.
        if (!data.generation.compare_exchange_strong(gen, gen + 1)) return;
        data.stream.store(nullptr, std::memory_order_release);
        if (waitForCallbacks) {
            for (int spins = 0; data.active.load() != 0; ) {
                if (spins < 100) {
                    ++spins;
                    std::this_thread::yield();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        if (index < state.slots.size()) {
            state.slots[index].clearControl();
            state.slots[index].inUse = false;
        }
    }

    // Lock-free, wait-free RT entry point for AAudio data callbacks. Always pair with
    // leaveDataCallback(slot).
    static AudioStreamAAudio *enterDataCallback(void *userData,
                                                DataSlot **slotOut,
                                                uint32_t *expectedGeneration) {
        const auto handle = reinterpret_cast<uintptr_t>(userData);
        const uintptr_t handleGen = handle >> kIndexBits;
        if (handleGen == 0) {
            *slotOut = nullptr;
            return nullptr;
        }
        DataSlot *slot = &sDataSlots[handle & kIndexMask];
        *slotOut = slot;
        slot->active.fetch_add(1); // seq_cst
        const uint32_t gen = slot->generation.load(); // seq_cst
        if ((gen & kGenerationMask) != handleGen) {
            return nullptr;
        }
        *expectedGeneration = gen;
        return slot->stream.load(std::memory_order_acquire);
    }

    static void leaveDataCallback(DataSlot *slot) {
        if (slot != nullptr) {
            slot->active.fetch_sub(1, std::memory_order_release);
        }
    }

    // Control-plane lookup for error/routing/presentation/stop callbacks. Never dereferences
    // userData; returns a HelperLease taken under the same lock so raw-pointer streams stay alive.
    static CallbackRefs lookupControl(void *userData) {
        const auto handle = reinterpret_cast<uintptr_t>(userData);
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        const ControlSlot *slot = findSlotLocked(state, handle);
        if (slot == nullptr || !slot->active) return {};
        if ((slot->usesParentSharedPtr && slot->weakParentStream.expired())
                || (slot->usesSharedPtr && slot->weakStream.expired())) {
            return {};
        }
        CallbackRefs refs;
        refs.stream = slot->oboeStream;
        refs.weakStream = slot->weakStream;
        refs.weakParentStream = slot->weakParentStream;
        refs.usesSharedPtr = slot->usesSharedPtr;
        refs.usesParentSharedPtr = slot->usesParentSharedPtr;
        refs.lease = HelperLease(slot->helperThreads);
        return refs;
    }

    // Closes every active stream during exit() before libaaudio's static destructors run.
    // Never invokes closeStream() or drops shared_ptrs while holding state.lock.
    template <typename CloseFn>
    static void drainForExit(CloseFn &&closeStream) {
        while (true) {
            AudioStreamAAudio *oboeStream = nullptr;
            std::shared_ptr<AudioStream> sharedStream;
            std::shared_ptr<AudioStream> sharedParentStream;
            HelperLease lease;
            bool found = false;
            {
                ControlState &state = control();
                std::lock_guard<std::mutex> lock(state.lock);
                for (ControlSlot &slot : state.slots) {
                    if (!slot.active) continue;
                    found = true;
                    bool alive = true;
                    if (slot.usesParentSharedPtr) {
                        sharedParentStream = slot.weakParentStream.lock();
                        alive = (sharedParentStream != nullptr);
                    }
                    if (alive && slot.usesSharedPtr) {
                        sharedStream = slot.weakStream.lock();
                        alive = (sharedStream != nullptr);
                    }
                    if (alive) {
                        oboeStream = slot.oboeStream;
                        lease = HelperLease(slot.helperThreads);
                    }
                    slot.clearControl();
                    break;
                }
            }
            if (!found) return;
            if (oboeStream != nullptr) {
                closeStream(oboeStream);
            }
            lease.reset();
        }
    }

    static int32_t getInUseCountForTest() {
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        int32_t count = 0;
        for (const ControlSlot &slot : state.slots) {
            count += slot.inUse ? 1 : 0;
        }
        return count;
    }

    static int32_t getAllocatedCountForTest() {
        ControlState &state = control();
        std::lock_guard<std::mutex> lock(state.lock);
        return static_cast<int32_t>(state.slots.size());
    }

private:
    // 10 index bits (1024 concurrent streams); remaining bits hold generation (22 bits on 32-bit,
    // 32 bits on 64-bit).
    static constexpr uint32_t kIndexBits = 10;
    static constexpr uint32_t kMaxSlots = 1u << kIndexBits;
    static constexpr uintptr_t kIndexMask = kMaxSlots - 1;
    static constexpr uintptr_t kGenerationMask = (sizeof(uintptr_t) >= 8)
            ? uintptr_t{0xFFFFFFFFu}
            : ((uintptr_t{1} << (32 - kIndexBits)) - 1);

    struct ControlSlot {
        bool inUse = false;
        bool active = false;
        AudioStreamAAudio *oboeStream = nullptr;
        const AudioStream *parentStream = nullptr; // identity only; never dereferenced
        std::weak_ptr<AudioStream> weakStream;
        std::weak_ptr<AudioStream> weakParentStream;
        bool usesSharedPtr = false;
        bool usesParentSharedPtr = false;
        std::shared_ptr<HelperThreadTracker> helperThreads;

        void clearControl() {
            active = false;
            oboeStream = nullptr;
            parentStream = nullptr;
            weakStream.reset();
            weakParentStream.reset();
            usesSharedPtr = false;
            usesParentSharedPtr = false;
            helperThreads.reset();
        }
    };

    struct ControlState {
        std::mutex lock;
        std::vector<ControlSlot> slots;
    };

    static ControlSlot *findSlotLocked(ControlState &state, uintptr_t handle) {
        const uintptr_t expectedGen = handle >> kIndexBits;
        if (expectedGen == 0) return nullptr;
        const uint32_t index = static_cast<uint32_t>(handle & kIndexMask);
        if (index >= state.slots.size() || !state.slots[index].inUse) return nullptr;
        if ((sDataSlots[index].generation.load(std::memory_order_acquire) & kGenerationMask)
                != expectedGen) {
            return nullptr;
        }
        return &state.slots[index];
    }

    static ControlState &control() {
        // Intentionally leaked so it outlives static/global destructors at exit()/dlclose().
        static auto *state = new ControlState();
        return *state;
    }

    static DataSlot sDataSlots[kMaxSlots];
};

inline AAudioStreamRegistry::DataSlot
        AAudioStreamRegistry::sDataSlots[AAudioStreamRegistry::kMaxSlots];

} // namespace oboe

#endif // OBOE_AAUDIO_STREAM_REGISTRY_H_
