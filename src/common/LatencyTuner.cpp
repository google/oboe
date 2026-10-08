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

    // In Idle state, absorb any startup / post-reset xRuns and re-baseline at the end.
    if (mState.load() == State::Idle) {
        if (mIdleCountDown.load() > 0) {
            if (xRunDelta > 0) {
                mSuppressedXRunCount.fetch_add(xRunDelta);
            }
            mPreviousXRuns = currentXRuns;
            mPendingXRunEvents = 0;
            if (mIdleCountDown.fetch_sub(1) - 1 <= 0) {
                mIdleCountDown.store(0);
                State expected = State::Idle;
                mState.compare_exchange_strong(expected, State::Active);
            }
            return Result::OK;
        }
        State expected = State::Idle;
        mState.compare_exchange_strong(expected, State::Active);
    }

    // In Settling state after a buffer size change, absorb additional xRun increments
    // from the same underrun episode while the new buffer size takes effect.
    if (mState.load() == State::Settling) {
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
        if (xRunDelta > 0) {
            mCalmCallbackCount = 0;
            if (currentState == State::AtMax) {
                int32_t maxBuf = mMaxBufferSize.load();
                if (maxBuf > 0 && mStream.getBufferSizeInFrames() < maxBuf) {
                    currentState = State::Active;
                    mState.store(State::Active);
                }
            }
            if (currentState == State::Active) {
                mPendingXRunEvents++;
                if (mPendingXRunEvents >= mXRunThreshold.load()) {
                    mPendingXRunEvents = 0;
                    int32_t oldBufferSize = mStream.getBufferSizeInFrames();
                    int32_t maxBuf = mMaxBufferSize.load();
                    if (maxBuf > 0 && oldBufferSize >= maxBuf) {
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
                            (maxBuf > 0 && newBufferSize >= maxBuf)) {
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
            int32_t callbacksBeforeStepDown = mCallbacksBeforeStepDown.load();
            if (callbacksBeforeStepDown > 0) {
                mCalmCallbackCount++;
                if (mCalmCallbackCount >= callbacksBeforeStepDown) {
                    mCalmCallbackCount = 0;
                    mPendingXRunEvents = 0;
                    if (mSteppedDownRecently) {
                        // Survived a full quiet window at the stepped-down size.
                        mSteppedDownRecently = false;
                    }
                    int32_t oldBufferSize = mStream.getBufferSizeInFrames();
                    int32_t minFloor = getEffectiveMinimumBufferSize();
                    if (oldBufferSize > minFloor) {
                        int32_t decrement = getBufferSizeDecrement();
                        if (decrement <= 0) {
                            decrement = getBufferSizeIncrement();
                        }
                        if (decrement <= 0) {
                            decrement = std::max(1, mStream.getFramesPerBurst());
                        }
                        int32_t requestedBufferSize = std::max(oldBufferSize - decrement, minFloor);
                        auto setBufferResult = mStream.setBufferSizeInFrames(requestedBufferSize);
                        if (setBufferResult != Result::OK) {
                            mState.store(State::Unsupported);
                            return setBufferResult.error();
                        }
                        int32_t newBufferSize = setBufferResult.value();
                        if (newBufferSize >= oldBufferSize) {
                            int32_t burstDecrement = std::max(1, mStream.getFramesPerBurst());
                            if (burstDecrement > decrement &&
                                    oldBufferSize - burstDecrement >= minFloor) {
                                requestedBufferSize =
                                        std::max(oldBufferSize - burstDecrement, minFloor);
                                setBufferResult =
                                        mStream.setBufferSizeInFrames(requestedBufferSize);
                                if (setBufferResult != Result::OK) {
                                    mState.store(State::Unsupported);
                                    return setBufferResult.error();
                                }
                                newBufferSize = setBufferResult.value();
                            }
                        }
                        if (newBufferSize < oldBufferSize) {
                            mStepDownCount.fetch_add(1);
                            mSteppedDownRecently = true;
                            mState.store(State::Active);
                        } else if (mStepDownBackoffEnabled.load()) {
                            mDynamicMinimumBufferSize.store(oldBufferSize);
                        }
                    }
                }
            } else {
                mCalmCallbackCount = 0;
                mSteppedDownRecently = false;
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
    mIdleCountDown.store(idleCount);
    mSettleCountDown = 0;
    mPendingXRunEvents = 0;
    mCalmCallbackCount = 0;
    mSteppedDownRecently = false;
    mBumpUpCount.store(0);
    mStepDownCount.store(0);
    mSuppressedXRunCount.store(0);
    mDynamicMinimumBufferSize.store(0);
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
    mPreviousXRuns = xRunCountResult.value();
    mState.store((idleCount > 0) ? State::Idle : State::Active);
}

bool LatencyTuner::isAtMaximumBufferSize() {
    return mState.load() == State::AtMax;
}
