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

#include <algorithm>
#include "oboe/LatencyTuner.h"

using namespace oboe;

LatencyTuner::LatencyTuner(AudioStream &stream)
        : LatencyTuner(stream, stream.getBufferCapacityInFrames()) {
}

LatencyTuner::LatencyTuner(oboe::AudioStream &stream, int32_t maximumBufferSize)
        : mStream(stream)
        , mMaxBufferSize(maximumBufferSize) {
    int32_t burstSize = stream.getFramesPerBurst();
    if (burstSize <= 0) {
        burstSize = 1;
    }
    int32_t capacity = stream.getBufferCapacityInFrames();
    int32_t maxBuf = maximumBufferSize;
    if (maxBuf <= 0) {
        maxBuf = (capacity > 0) ? capacity : 0;
    } else if (capacity > 0 && maxBuf > capacity) {
        maxBuf = capacity;
    }
    mMaxBufferSize.store(maxBuf);
    mLastKnownBufferSize.store(stream.getBufferSizeInFrames());
    int32_t minBufferSize = kDefaultNumBursts * burstSize;
    if (maxBuf > 0 && minBufferSize > maxBuf) {
        minBufferSize = maxBuf;
    }
    setMinimumBufferSize(minBufferSize);
    setBufferSizeIncrement(burstSize);
    setBufferSizeDecrement(burstSize);
    reset();
}

Result LatencyTuner::tune() {
    if (mState.load() == State::Unsupported) {
        return Result::ErrorUnimplemented;
    }
    if (!mStream.isXRunCountSupported()) {
        mState.store(State::Unsupported);
        return Result::ErrorUnimplemented;
    }

    // Process reset requests.
    int32_t numRequests = mLatencyTriggerRequests.load();
    if (numRequests != mLatencyTriggerResponses.load()) {
        mLatencyTriggerResponses.store(numRequests);
        reset();
        if (mState.load() == State::Unsupported) {
            return Result::ErrorUnimplemented;
        }
    }

    auto xRunCountResult = mStream.getXRunCount();
    if (xRunCountResult != Result::OK) {
        mState.store(State::Unsupported);
        return Result::ErrorUnimplemented;
    }
    const int32_t currentXRuns = xRunCountResult.value();
    const int32_t xRunDelta = currentXRuns - mPreviousXRuns;

    // In Idle state, absorb any startup / post-reset xRuns and re-baseline on every callback.
    // Only tune() leaves Idle, so the baseline is always latched before monitoring starts.
    if (mState.load() == State::Idle) {
        mLastKnownBufferSize.store(mStream.getBufferSizeInFrames());
        if (xRunDelta > 0) {
            mSuppressedXRunCount.fetch_add(xRunDelta);
        }
        mPreviousXRuns = currentXRuns;
        mPendingXRunEvents = 0;
        mInXRunEpisode = false;
        if (mIdleCallbackCount < INT32_MAX) {
            ++mIdleCallbackCount;
        }
        if (mIdleCallbackCount >= mIdleCount.load()) {
            State expected = State::Idle;
            mState.compare_exchange_strong(expected, State::Active);
        }
        return Result::OK;
    }

    // In Settling state after a buffer size increase, absorb additional xRun increments
    // from the same underrun episode while the new buffer size takes effect.
    if (mState.load() == State::Settling) {
        mLastKnownBufferSize.store(mStream.getBufferSizeInFrames());
        mInXRunEpisode = false;
        if (mSettleCountDown > 0) {
            if (xRunDelta > 0) {
                mSuppressedXRunCount.fetch_add(xRunDelta);
            }
            mPreviousXRuns = currentXRuns;
            mPendingXRunEvents = 0;
            if (--mSettleCountDown <= 0) {
                mState.store(State::Active);
            }
            return Result::OK;
        }
        mState.store(State::Active);
    }

    mPreviousXRuns = currentXRuns;

    State currentState = mState.load();
    if (currentState == State::Active || currentState == State::AtMax) {
        int32_t currentBufferSize = mStream.getBufferSizeInFrames();
        mLastKnownBufferSize.store(currentBufferSize);
        int32_t effectiveMax = getEffectiveMaximumBufferSize();
        if (currentState == State::AtMax && effectiveMax > 0
                && currentBufferSize < effectiveMax) {
            currentState = State::Active;
            mState.store(State::Active);
        }

        if (xRunDelta > 0) {
            mCalmCallbackCount = 0;
            const bool isRisingEdge = !mInXRunEpisode;
            mInXRunEpisode = true;
            if (currentState == State::Active) {
                if (isRisingEdge) {
                    mPendingXRunEvents++;
                }
                if (mPendingXRunEvents >= mXRunThreshold.load()) {
                    mPendingXRunEvents = 0;
                    mInXRunEpisode = false;
                    int32_t oldBufferSize = currentBufferSize;
                    int32_t maxBuf = mMaxBufferSize.load();
                    if (effectiveMax > 0 && oldBufferSize >= effectiveMax) {
                        mSteppedDownRecently = false;
                        mState.store(State::AtMax);
                        return Result::OK;
                    }
                    int32_t increment = getBufferSizeIncrement();
                    if (increment <= 0) {
                        increment = std::max(1, mStream.getFramesPerBurst());
                    }
                    int64_t requested64 =
                            static_cast<int64_t>(oldBufferSize) + static_cast<int64_t>(increment);

                    // Do not request more than the maximum buffer size (which was either
                    // user-specified or was from stream->getBufferCapacityInFrames()).
                    if (maxBuf > 0 && requested64 > maxBuf) {
                        requested64 = maxBuf;
                    }
                    if (requested64 > INT32_MAX) {
                        requested64 = INT32_MAX;
                    }
                    int32_t requestedBufferSize = static_cast<int32_t>(requested64);

                    // Note that this will not allocate more memory. It simply determines
                    // how much of the existing buffer capacity will be used. The size will be
                    // clipped to the bufferCapacity by AAudio.
                    auto setBufferResult = mStream.setBufferSizeInFrames(requestedBufferSize);
                    if (setBufferResult != Result::OK) {
                        mState.store(State::Unsupported);
                        return setBufferResult.error();
                    }
                    int32_t newBufferSize = setBufferResult.value();
                    if (newBufferSize <= oldBufferSize) {
                        int32_t burstIncrement = std::max(1, mStream.getFramesPerBurst());
                        if (burstIncrement > increment) {
                            int64_t burstRequested64 =
                                    static_cast<int64_t>(oldBufferSize) +
                                    static_cast<int64_t>(burstIncrement);
                            if (maxBuf > 0 && burstRequested64 > maxBuf) {
                                burstRequested64 = maxBuf;
                            }
                            if (burstRequested64 > INT32_MAX) {
                                burstRequested64 = INT32_MAX;
                            }
                            int32_t burstRequestedBufferSize =
                                    static_cast<int32_t>(burstRequested64);
                            if (burstRequestedBufferSize > requestedBufferSize) {
                                setBufferResult =
                                        mStream.setBufferSizeInFrames(burstRequestedBufferSize);
                                if (setBufferResult != Result::OK) {
                                    mState.store(State::Unsupported);
                                    return setBufferResult.error();
                                }
                                newBufferSize = setBufferResult.value();
                            }
                        }
                    }
                    mLastKnownBufferSize.store(newBufferSize);
                    if (newBufferSize <= oldBufferSize && newBufferSize > 0) {
                        // Stream refused further growth below mMaxBufferSize (e.g. AAudio MMAP
                        // caps buffer size at capacity - 1 burst, or Samsung exclusive at
                        // capacity - 2 bursts). Latch the observed ceiling so AtMax holds without
                        // flapping or repeatedly calling setBufferSizeInFrames().
                        mObservedMaxBufferSize.store(newBufferSize);
                        effectiveMax = getEffectiveMaximumBufferSize();
                    }

                    if (mSteppedDownRecently) {
                        if (mStepDownBackoffEnabled.load() && newBufferSize > oldBufferSize) {
                            int32_t curDyn = mDynamicMinimumBufferSize.load();
                            if (newBufferSize > curDyn) {
                                mDynamicMinimumBufferSize.store(newBufferSize);
                            }
                        }
                        mSteppedDownRecently = false;
                    }

                    if (newBufferSize > oldBufferSize) {
                        mBumpUpCount.fetch_add(1);
                    }
                    int32_t settleCount = mSettleCount.load();
                    if (newBufferSize <= oldBufferSize ||
                            (effectiveMax > 0 && newBufferSize >= effectiveMax)) {
                        mState.store(State::AtMax);
                    } else if (settleCount > 0) {
                        mState.store(State::Settling);
                        mSettleCountDown = settleCount;
                    } else {
                        mState.store(State::Active);
                    }
                }
            } else {
                // AtMax and still seeing xRuns.
                mSteppedDownRecently = false;
            }
        } else {
            mInXRunEpisode = false;
            int32_t callbacksBeforeStepDown = mCallbacksBeforeStepDown.load();
            if (callbacksBeforeStepDown > 0) {
                if (mCalmCallbackCount < INT32_MAX) {
                    mCalmCallbackCount++;
                }
                if (mCalmCallbackCount >= callbacksBeforeStepDown) {
                    mCalmCallbackCount = 0;
                    mPendingXRunEvents = 0;
                    if (mSteppedDownRecently) {
                        // Survived a full quiet window at the stepped-down size.
                        mSteppedDownRecently = false;
                    }
                    int32_t oldBufferSize = currentBufferSize;
                    int32_t minFloor = getEffectiveMinimumBufferSize();
                    if (oldBufferSize > minFloor) {
                        int32_t decrement = getBufferSizeDecrement();
                        if (decrement <= 0) {
                            decrement = getBufferSizeIncrement();
                        }
                        if (decrement <= 0) {
                            decrement = std::max(1, mStream.getFramesPerBurst());
                        }
                        int64_t requested64 =
                                static_cast<int64_t>(oldBufferSize) -
                                static_cast<int64_t>(decrement);
                        int32_t requestedBufferSize = (requested64 < minFloor)
                                ? minFloor
                                : static_cast<int32_t>(requested64);
                        auto setBufferResult = mStream.setBufferSizeInFrames(requestedBufferSize);
                        if (setBufferResult != Result::OK) {
                            mState.store(State::Unsupported);
                            return setBufferResult.error();
                        }
                        int32_t newBufferSize = setBufferResult.value();
                        if (newBufferSize >= oldBufferSize) {
                            int32_t burstDecrement = std::max(1, mStream.getFramesPerBurst());
                            if (burstDecrement > decrement) {
                                int64_t burstRequested64 =
                                        static_cast<int64_t>(oldBufferSize) -
                                        static_cast<int64_t>(burstDecrement);
                                int32_t burstRequestedBufferSize = (burstRequested64 < minFloor)
                                        ? minFloor
                                        : static_cast<int32_t>(burstRequested64);
                                if (burstRequestedBufferSize < requestedBufferSize) {
                                    setBufferResult =
                                            mStream.setBufferSizeInFrames(burstRequestedBufferSize);
                                    if (setBufferResult != Result::OK) {
                                        mState.store(State::Unsupported);
                                        return setBufferResult.error();
                                    }
                                    newBufferSize = setBufferResult.value();
                                }
                            }
                        }
                        mLastKnownBufferSize.store(newBufferSize);
                        if (newBufferSize < oldBufferSize) {
                            mStepDownCount.fetch_add(1);
                            mSteppedDownRecently = true;
                            mState.store(State::Active);
                        } else {
                            // The stream refused to shrink (e.g. HAL minimum or burst
                            // quantization). Record the floor, independent of step-down backoff,
                            // so later quiet windows do not keep calling setBufferSizeInFrames().
                            mDynamicMinimumBufferSize.store(oldBufferSize);
                        }
                    }
                }
            } else {
                mSteppedDownRecently = false;
                if (mPendingXRunEvents > 0) {
                    if (mCalmCallbackCount < INT32_MAX) {
                        mCalmCallbackCount++;
                    }
                    int64_t decayWindow = std::max<int64_t>(
                            static_cast<int64_t>(std::max(0, mSettleCount.load())) * 8, 100);
                    if (mCalmCallbackCount >= decayWindow) {
                        mPendingXRunEvents = 0;
                        mCalmCallbackCount = 0;
                    }
                } else {
                    mCalmCallbackCount = 0;
                }
            }
        }
    }

    return Result::OK;
}

void LatencyTuner::requestReset() {
    if (mState.load() != State::Unsupported) {
        mLatencyTriggerRequests++;
    }
}

void LatencyTuner::reset() {
    int32_t idleCount = mIdleCount.load();
    mIdleCallbackCount = 0;
    mSettleCountDown = 0;
    mPendingXRunEvents = 0;
    mInXRunEpisode = false;
    mCalmCallbackCount = 0;
    mSteppedDownRecently = false;
    mBumpUpCount.store(0);
    mStepDownCount.store(0);
    mSuppressedXRunCount.store(0);
    mDynamicMinimumBufferSize.store(0);
    mObservedMaxBufferSize.store(0);
    if (!mStream.isXRunCountSupported()) {
        mState.store(State::Unsupported);
        return;
    }
    auto xRunCountResult = mStream.getXRunCount();
    if (xRunCountResult != Result::OK) {
        mState.store(State::Unsupported);
        return;
    }
    // Set to minimal latency
    int32_t minBuf = getMinimumBufferSize();
    int32_t maxBuf = getMaximumBufferSize();
    if (maxBuf > 0 && minBuf > maxBuf) {
        minBuf = maxBuf;
    }
    auto setBufferResult = mStream.setBufferSizeInFrames(minBuf);
    if (setBufferResult != Result::OK) {
        mState.store(State::Unsupported);
        return;
    }
    mLastKnownBufferSize.store(setBufferResult.value());
    mPreviousXRuns = xRunCountResult.value();
    mState.store((idleCount > 0) ? State::Idle : State::Active);
}

bool LatencyTuner::isAtMaximumBufferSize() {
    if (mState.load() != State::AtMax) {
        return false;
    }
    int32_t effectiveMax = getEffectiveMaximumBufferSize();
    int32_t lastKnown = mLastKnownBufferSize.load();
    return effectiveMax <= 0 || lastKnown >= effectiveMax;
}
