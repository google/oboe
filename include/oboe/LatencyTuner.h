/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef OBOE_LATENCY_TUNER_
#define OBOE_LATENCY_TUNER_

#include <atomic>
#include <cstdint>
#include "oboe/Definitions.h"
#include "oboe/AudioStream.h"

namespace oboe {

/**
 * LatencyTuner can be used to dynamically tune the latency of an output stream.
 * It adjusts the stream's bufferSize by monitoring the number of underruns.
 *
 * This only affects the latency associated with the first level of buffering that is closest
 * to the application. It does not affect low latency in the HAL, or touch latency in the UI.
 *
 * Call tune() right before returning from your data callback function if using callbacks.
 * Call tune() right before calling write() if using blocking writes.
 *
 * If you want to see the ongoing results of this tuning process then call
 * stream->getBufferSize() periodically.
 *
 * Thread safety:
 * - tune() must be called from a single thread (the audio data callback or the blocking I/O
 *   thread), never concurrently from multiple threads.
 * - requestReset(), all configuration setters, and all getters / telemetry accessors are
 *   lock-free and safe to call concurrently with tune() from any thread (e.g. a UI thread).
 */
class LatencyTuner {
public:

    /**
     * Internal state of the LatencyTuner state machine.
     */
    enum class State : int32_t {
        /**
         * Initial grace period after construction or reset(). XRuns during this
         * window are ignored and the baseline xRun count is latched on each Idle
         * callback until transitioning to State::Active.
         */
        Idle = 0,
        /**
         * Actively monitoring getXRunCount() for underruns.
         */
        Active = 1,
        /**
         * Cooldown window after increasing the buffer size. Additional xRun
         * increments from the same underrun episode are absorbed while the new
         * buffer size takes effect.
         */
        Settling = 2,
        /**
         * The stream buffer size has reached maximumBufferSize (or capacity).
         */
        AtMax = 3,
        /**
         * Latency tuning is not supported on this stream (e.g. OpenSL ES).
         */
        Unsupported = 4
    };

    /**
     * Construct a new LatencyTuner object which will act on the given audio stream
     *
     * @param stream the stream who's latency will be tuned
     */
    explicit LatencyTuner(AudioStream &stream);

    /**
     * Construct a new LatencyTuner object which will act on the given audio stream.
     *
     * @param stream the stream who's latency will be tuned
     * @param maximumBufferSize the maximum buffer size which the tune() operation will set the buffer size to
     */
    explicit LatencyTuner(AudioStream &stream, int32_t maximumBufferSize);

    /**
     * Adjust the bufferSizeInFrames to optimize latency.
     * It will start with a low latency and then raise it if an underrun occurs.
     *
     * Latency tuning is only supported for AAudio.
     *
     * @return OK or negative error, ErrorUnimplemented for OpenSL ES
     */
    Result tune();

    /**
     * This may be called from another thread. Then tune() will call reset(),
     * which will lower the latency to the minimum and then allow it to rise back up
     * if there are glitches.
     *
     * This is typically called in response to a user decision to minimize latency. In other words,
     * call this from a button handler.
     */
    void requestReset();

    /**
     * @return true if the audio stream's buffer size is at the maximum value. If no maximum value
     * was specified when constructing the LatencyTuner then the value of
     * stream->getBufferCapacityInFrames is used
     */
    bool isAtMaximumBufferSize();

    /**
     * Set the minimum bufferSize in frames that is used when the tuner is reset or steps down.
     * Any higher dynamic floor learned from step-down backoff hysteresis is preserved until
     * requestReset() or setStepDownBackoffEnabled(false) is called, so updating this value from
     * a UI slider does not discard learned stability floors. Call requestReset() after this if
     * you want to immediately drop the stream buffer size to the new minimum.
     * @param bufferSize
     */
    void setMinimumBufferSize(int32_t bufferSize) {
        int32_t clamped = (bufferSize < 0) ? 0 : bufferSize;
        mMinimumBufferSize.store(clamped);
        leaveAtMaxIfBelow(getEffectiveMaximumBufferSize());
    }

    int32_t getMinimumBufferSize() const {
        return mMinimumBufferSize.load();
    }

    /**
     * Set the maximum bufferSize in frames that tune() will request.
     * @param maxBufferSize maximum buffer size in frames
     */
    void setMaximumBufferSize(int32_t maxBufferSize) {
        int32_t capacity = mStream.getBufferCapacityInFrames();
        int32_t clamped = maxBufferSize;
        if (clamped <= 0) {
            clamped = (capacity > 0) ? capacity : 0;
        } else if (capacity > 0 && clamped > capacity) {
            clamped = capacity;
        }
        mObservedMaxBufferSize.store(0);
        mMaxBufferSize.store(clamped);
        leaveAtMaxIfBelow(clamped);
    }

    int32_t getMaximumBufferSize() const {
        return mMaxBufferSize.load();
    }

    /**
     * Set the amount the bufferSize will be incremented while tuning.
     * By default, this will be one burst.
     *
     * Note that AAudio will quantize the buffer size to a multiple of the burstSize.
     * So the final buffer sizes may not be a multiple of this increment.
     *
     * @param sizeIncrement
     */
    void setBufferSizeIncrement(int32_t sizeIncrement) {
        mBufferSizeIncrement.store(sizeIncrement);
    }

    int32_t getBufferSizeIncrement() const {
        return mBufferSizeIncrement.load();
    }

    /**
     * Set the number of tune() calls to wait in State::Idle after construction or reset()
     * before actively monitoring xRuns. Any xRuns occurring during this startup grace period
     * are ignored and the xRun baseline is latched when transitioning to State::Active.
     *
     * Default is kDefaultIdleCount (8). Set to 0 before requestReset() to disable the
     * post-reset grace period; calling setIdleCount(0) while already in State::Idle (for
     * example immediately after construction) absorbs at most one tune() call so startup xRuns
     * are still baselined before entering State::Active.
     *
     * The window is measured in tune() calls since construction or the last reset() and is
     * re-evaluated by tune() on every call, so changing it while in State::Idle shortens or
     * extends the current window, and re-applying the same value has no effect. Only tune()
     * leaves State::Idle, after absorbing pending xRuns and re-latching the xRun baseline.
     *
     * @param idleCount number of callbacks to stay idle after reset
     */
    void setIdleCount(int32_t idleCount) {
        mIdleCount.store((idleCount < 0) ? 0 : idleCount);
    }

    int32_t getIdleCount() const {
        return mIdleCount.load();
    }

    /**
     * Set the number of tune() calls to wait in State::Settling after each buffer size
     * increase before responding to further xRun increases.
     *
     * On many Android devices, a single underrun episode increments the stream's xRun counter
     * on multiple consecutive callbacks while the ring buffer recovers, and increasing the
     * buffer size takes several callbacks to fill to the new threshold. Waiting for a settle
     * window prevents a single glitch from bumping the buffer size multiple times in a row.
     *
     * Default is kDefaultSettleCount (8). Set to 0 for legacy behavior (no settling cooldown).
     *
     * @param settleCount number of callbacks to settle after each buffer size increase
     */
    void setSettleCount(int32_t settleCount) {
        mSettleCount.store((settleCount < 0) ? 0 : settleCount);
    }

    int32_t getSettleCount() const {
        return mSettleCount.load();
    }

    /**
     * Set the number of distinct xRun episodes required in State::Active before increasing
     * the buffer size. Consecutive callbacks reporting xRun increments are counted as a single
     * episode (rising-edge detection), and pending episode counts below the threshold decay
     * back to 0 after a sustained glitch-free window so isolated glitches hours apart do not
     * accumulate. Default is 1.
     *
     * @param threshold number of xRun episodes (>= 1)
     */
    void setXRunThreshold(int32_t threshold) {
        mXRunThreshold.store((threshold < 1) ? 1 : threshold);
    }

    int32_t getXRunThreshold() const {
        return mXRunThreshold.load();
    }

    /**
     * Set the number of consecutive glitch-free tune() calls in State::Active (or State::AtMax)
     * before automatically stepping the buffer size down toward the minimum buffer size.
     *
     * By default this is 0 (disabled), meaning the buffer size only increases until
     * requestReset() is called. Setting a positive value (for example, 1000 callbacks, which is
     * ~2 seconds at 2 ms per burst) allows the stream to recover low latency after a transient
     * CPU spike.
     *
     * @param callbacksBeforeStepDown number of glitch-free callbacks before stepping down, or 0 to disable
     */
    void setCallbacksBeforeStepDown(int32_t callbacksBeforeStepDown) {
        mCallbacksBeforeStepDown.store((callbacksBeforeStepDown < 0) ? 0 : callbacksBeforeStepDown);
    }

    int32_t getCallbacksBeforeStepDown() const {
        return mCallbacksBeforeStepDown.load();
    }

    /**
     * Set the amount by which the buffer size is decreased when stepping down after
     * getCallbacksBeforeStepDown() glitch-free callbacks. Default is one burst.
     *
     * @param sizeDecrement decrement in frames
     */
    void setBufferSizeDecrement(int32_t sizeDecrement) {
        mBufferSizeDecrement.store(sizeDecrement);
    }

    int32_t getBufferSizeDecrement() const {
        return mBufferSizeDecrement.load();
    }

    /**
     * Enable or disable dynamic-floor hysteresis when stepping down.
     *
     * When enabled (the default), if stepping down to a smaller buffer size is followed by an
     * xRun before completing another full step-down window, the tuner bumps the buffer size back
     * up and records that higher size as the dynamic minimum floor (until requestReset() is
     * called). This prevents periodic sawtooth oscillation and repeated glitches at the
     * stability boundary.
     *
     * @param enabled true to lock the dynamic minimum floor after a failed step-down
     */
    void setStepDownBackoffEnabled(bool enabled) {
        mStepDownBackoffEnabled.store(enabled);
        if (!enabled) {
            mDynamicMinimumBufferSize.store(0);
        }
    }

    bool isStepDownBackoffEnabled() const {
        return mStepDownBackoffEnabled.load();
    }

    /**
     * @return current state of the LatencyTuner
     */
    State getState() const {
        return mState.load();
    }

    /**
     * @return total number of times tune() increased the stream's buffer size since construction
     *         or the last reset()
     */
    int32_t getBumpUpCount() const {
        return mBumpUpCount.load();
    }

    /**
     * @return total number of times tune() decreased the stream's buffer size since construction
     *         or the last reset()
     */
    int32_t getStepDownCount() const {
        return mStepDownCount.load();
    }

    /**
     * @return total number of xRun increments absorbed during State::Idle or State::Settling
     *         windows since construction or the last reset()
     */
    int32_t getSuppressedXRunCount() const {
        return mSuppressedXRunCount.load();
    }

    /**
     * @return the effective minimum buffer size in frames: the larger of getMinimumBufferSize()
     *         and the dynamic floor. The dynamic floor is raised by step-down backoff hysteresis,
     *         or (regardless of backoff) when a step-down request is not honored because the
     *         stream cannot shrink below its current size. It is cleared by requestReset()
     *         and setStepDownBackoffEnabled(false).
     */
    int32_t getEffectiveMinimumBufferSize() const {
        int32_t minBuf = mMinimumBufferSize.load();
        int32_t dynBuf = mDynamicMinimumBufferSize.load();
        return (dynBuf > minBuf) ? dynBuf : minBuf;
    }

    static constexpr int32_t kIdleCount = 8;
    static constexpr int32_t kDefaultIdleCount = kIdleCount;
    static constexpr int32_t kDefaultSettleCount = 8;
    static constexpr int32_t kDefaultNumBursts = 2;

private:

    /**
     * Drop the latency down to the minimum and then let it rise back up.
     * This is useful if a glitch caused the latency to increase and it hasn't gone back down.
     *
     * This should only be called in the same thread as tune().
     */
    void reset();

    int32_t getEffectiveMaximumBufferSize() const {
        int32_t maxBuf = mMaxBufferSize.load();
        int32_t observedMax = mObservedMaxBufferSize.load();
        if (observedMax > 0 && (maxBuf <= 0 || observedMax < maxBuf)) {
            return observedMax;
        }
        return maxBuf;
    }

    /**
     * Leave State::AtMax only if the stream's last known buffer size is below effectiveMax.
     * Uses mLastKnownBufferSize (updated on the tune()/reset() thread) rather than calling
     * non-thread-safe mStream.getBufferSizeInFrames() from arbitrary caller threads.
     */
    void leaveAtMaxIfBelow(int32_t effectiveMax) {
        int32_t lastKnown = mLastKnownBufferSize.load();
        if (mState.load() == State::AtMax && effectiveMax > 0
                && lastKnown > 0 && lastKnown < effectiveMax) {
            State expected = State::AtMax;
            mState.compare_exchange_strong(expected, State::Active);
        }
    }

    AudioStream           &mStream;
    std::atomic<State>    mState{State::Idle};
    std::atomic<int32_t>  mMaxBufferSize{0};
    std::atomic<int32_t>  mObservedMaxBufferSize{0};
    std::atomic<int32_t>  mLastKnownBufferSize{0};
    int32_t               mPreviousXRuns = 0;
    std::atomic<int32_t>  mIdleCount{kDefaultIdleCount};
    int32_t               mIdleCallbackCount = 0; // only accessed by tune() and reset()
    std::atomic<int32_t>  mSettleCount{kDefaultSettleCount};
    int32_t               mSettleCountDown = 0;
    std::atomic<int32_t>  mXRunThreshold{1};
    int32_t               mPendingXRunEvents = 0;
    bool                  mInXRunEpisode = false;
    std::atomic<int32_t>  mCallbacksBeforeStepDown{0};
    int32_t               mCalmCallbackCount = 0;
    std::atomic<int32_t>  mMinimumBufferSize{0};
    std::atomic<int32_t>  mDynamicMinimumBufferSize{0};
    std::atomic<int32_t>  mBufferSizeIncrement{0};
    std::atomic<int32_t>  mBufferSizeDecrement{0};
    std::atomic<bool>     mStepDownBackoffEnabled{true};
    bool                  mSteppedDownRecently = false;
    std::atomic<int32_t>  mBumpUpCount{0};
    std::atomic<int32_t>  mStepDownCount{0};
    std::atomic<int32_t>  mSuppressedXRunCount{0};
    std::atomic<int32_t>  mLatencyTriggerRequests{0}; // TODO user atomic requester from AAudio
    std::atomic<int32_t>  mLatencyTriggerResponses{0};
};

} // namespace oboe

#endif // OBOE_LATENCY_TUNER_
