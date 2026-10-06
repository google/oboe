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

#include <algorithm>
#include <cstdint>
#include <thread>
#include <gtest/gtest.h>
#include <oboe/AudioStream.h>
#include <oboe/LatencyTuner.h>

using namespace oboe;

namespace {

/**
 * Deterministic in-memory AudioStream implementation for unit testing LatencyTuner
 * without hardware timing dependencies.
 */
class FakeAudioStream : public AudioStream {
public:
    FakeAudioStream(int32_t framesPerBurst = 96,
                    int32_t initialBufferSize = 192,
                    int32_t bufferCapacity = 960) {
        mFramesPerBurst = framesPerBurst;
        mBufferSizeInFrames = initialBufferSize;
        mBufferCapacityInFrames = bufferCapacity;
        mSampleRate = 48000;
        mChannelCount = 2;
        mFormat = AudioFormat::Float;
        mDirection = Direction::Output;
    }

    Result requestStart() override { return Result::OK; }
    Result requestPause() override { return Result::OK; }
    Result requestFlush() override { return Result::OK; }
    Result requestStop() override { return Result::OK; }
    StreamState getState() override { return StreamState::Started; }

    Result waitForStateChange(StreamState /* inputState */,
                              StreamState *nextState,
                              int64_t /* timeoutNanoseconds */) override {
        if (nextState != nullptr) {
            *nextState = StreamState::Started;
        }
        return Result::OK;
    }

    ResultWithValue<int32_t> setBufferSizeInFrames(int32_t requestedFrames) override {
        if (!mSetBufferSizeSupported) {
            return ResultWithValue<int32_t>(Result::ErrorUnimplemented);
        }
        mSetBufferSizeCallCount++;
        int32_t clamped = std::clamp(requestedFrames, mFramesPerBurst, mBufferCapacityInFrames);
        if (mQuantizeToBurst && mFramesPerBurst > 0) {
            // Simulate AAudio / HAL rounding buffer size to burst multiples or clamping.
            clamped = ((clamped + mFramesPerBurst - 1) / mFramesPerBurst) * mFramesPerBurst;
            clamped = std::min(clamped, mBufferCapacityInFrames);
        }
        mBufferSizeInFrames = clamped;
        return ResultWithValue<int32_t>(mBufferSizeInFrames);
    }

    ResultWithValue<int32_t> getXRunCount() override {
        if (!mXRunSupported) {
            return ResultWithValue<int32_t>(Result::ErrorUnimplemented);
        }
        return ResultWithValue<int32_t>(mXRunCount);
    }

    bool isXRunCountSupported() const override { return mXRunSupported; }
    AudioApi getAudioApi() const override { return AudioApi::AAudio; }
    void updateFramesWritten() override {}
    void updateFramesRead() override {}

    void addXRuns(int32_t delta = 1) { mXRunCount += delta; }
    void setXRunCount(int32_t count) { mXRunCount = count; }
    void setXRunSupported(bool supported) { mXRunSupported = supported; }
    void setSetBufferSizeSupported(bool supported) { mSetBufferSizeSupported = supported; }
    void setQuantizeToBurst(bool quantize) { mQuantizeToBurst = quantize; }
    int32_t getSetBufferSizeCallCount() const { return mSetBufferSizeCallCount; }

private:
    int32_t mXRunCount = 0;
    int32_t mSetBufferSizeCallCount = 0;
    bool mXRunSupported = true;
    bool mSetBufferSizeSupported = true;
    bool mQuantizeToBurst = false;
};

} // namespace

class LatencyTunerTest : public ::testing::Test {
protected:
    static constexpr int32_t kBurst = 96;
    static constexpr int32_t kInitialSize = 2 * kBurst; // 192
    static constexpr int32_t kCapacity = 8 * kBurst;    // 768

    FakeAudioStream mStream{kBurst, kInitialSize, kCapacity};
};

TEST_F(LatencyTunerTest, DefaultConfigurationAndStartupIdle) {
    LatencyTuner tuner(mStream);

    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Idle);
    EXPECT_EQ(tuner.getMinimumBufferSize(), 2 * kBurst);
    EXPECT_EQ(tuner.getEffectiveMinimumBufferSize(), 2 * kBurst);
    EXPECT_EQ(tuner.getMaximumBufferSize(), kCapacity);
    EXPECT_EQ(tuner.getBufferSizeIncrement(), kBurst);
    EXPECT_EQ(tuner.getIdleCount(), 8);
    EXPECT_EQ(tuner.getSettleCount(), 8);
    EXPECT_EQ(tuner.getXRunThreshold(), 1);
    EXPECT_EQ(tuner.getCallbacksBeforeStepDown(), 0);
    EXPECT_TRUE(tuner.isStepDownBackoffEnabled());
    EXPECT_FALSE(tuner.isAtMaximumBufferSize());

    // First 7 tune() calls remain in Idle; 8th transitions to Active.
    for (int i = 0; i < 7; ++i) {
        EXPECT_EQ(tuner.tune(), Result::OK);
        EXPECT_EQ(tuner.getState(), LatencyTuner::State::Idle);
    }
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 0);
}

TEST_F(LatencyTunerTest, StartupXRunsSuppressedAndBaselinedOnActiveTransition) {
    LatencyTuner tuner(mStream);

    // Simulate cold-start underruns occurring while the stream is still in its Idle window.
    mStream.addXRuns(3);
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(tuner.getSuppressedXRunCount(), 3);

    mStream.addXRuns(2);
    for (int i = 1; i < 8; ++i) {
        EXPECT_EQ(tuner.tune(), Result::OK);
    }
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);
    EXPECT_EQ(tuner.getSuppressedXRunCount(), 5);

    // First active callback with no *new* xRuns must NOT falsely bump buffer size!
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 0);
}

TEST_F(LatencyTunerTest, SingleGlitchMultiCallbackXRunBurstBumpsOnce) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(2);
    tuner.setSettleCount(4);

    // Drain Idle window (2 callbacks).
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(tuner.tune(), Result::OK);
    ASSERT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // Simulate a single severe scheduling stall where the ring buffer drains and
    // AAudio/Legacy increments xRunCount across 4 consecutive callbacks while refilling.
    mStream.addXRuns(2);
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 1);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Settling);

    // Remaining 3 callbacks of the same glitch episode increment xRunCount during Settling.
    for (int i = 0; i < 3; ++i) {
        mStream.addXRuns(1);
        EXPECT_EQ(tuner.tune(), Result::OK);
        EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
        EXPECT_EQ(tuner.getBumpUpCount(), 1);
    }
    EXPECT_EQ(tuner.getSuppressedXRunCount(), 3);

    // 4th settling callback completes the cooldown and returns to Active.
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 1);
}

TEST_F(LatencyTunerTest, LegacyModeZeroSettleBumpsEveryCallback) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(1);
    tuner.setSettleCount(0); // Disable post-bump settling cooldown

    EXPECT_EQ(tuner.tune(), Result::OK);
    ASSERT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // 3 consecutive callbacks with xRun increments should bump 3 times when settleCount == 0.
    for (int i = 1; i <= 3; ++i) {
        mStream.addXRuns(1);
        EXPECT_EQ(tuner.tune(), Result::OK);
        EXPECT_EQ(mStream.getBufferSizeInFrames(), (2 + i) * kBurst);
        EXPECT_EQ(tuner.getBumpUpCount(), i);
    }
}

TEST_F(LatencyTunerTest, DistinctGlitchesSeparatedBySettleWindowEachBumpUntilMax) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(1);
    tuner.setSettleCount(2);

    EXPECT_EQ(tuner.tune(), Result::OK);
    ASSERT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // Capacity is 8 bursts, starting at 2 bursts -> 6 distinct glitches to reach AtMax.
    for (int step = 1; step <= 6; ++step) {
        mStream.addXRuns(1);
        EXPECT_EQ(tuner.tune(), Result::OK);
        EXPECT_EQ(mStream.getBufferSizeInFrames(), (2 + step) * kBurst);
        EXPECT_EQ(tuner.getBumpUpCount(), step);

        if (step < 6) {
            EXPECT_EQ(tuner.getState(), LatencyTuner::State::Settling);
            // Wait out 2 settling callbacks.
            EXPECT_EQ(tuner.tune(), Result::OK);
            EXPECT_EQ(tuner.tune(), Result::OK);
            EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);
        } else {
            EXPECT_EQ(tuner.getState(), LatencyTuner::State::AtMax);
            EXPECT_TRUE(tuner.isAtMaximumBufferSize());
        }
    }

    // Further xRuns at AtMax should not call setBufferSizeInFrames again.
    int32_t callsBefore = mStream.getSetBufferSizeCallCount();
    mStream.addXRuns(5);
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getSetBufferSizeCallCount(), callsBefore);
    EXPECT_TRUE(tuner.isAtMaximumBufferSize());
}

TEST_F(LatencyTunerTest, CustomXRunThresholdRequiresMultipleEventsBeforeBump) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(1);
    tuner.setSettleCount(2);
    tuner.setXRunThreshold(3);

    EXPECT_EQ(tuner.tune(), Result::OK);
    ASSERT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // Event 1 (even if xRunCount jumps by 4 in a single callback, it counts as 1 event).
    mStream.addXRuns(4);
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 0);

    // Event 2.
    mStream.addXRuns(1);
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 0);

    // Event 3 triggers the bump!
    mStream.addXRuns(1);
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 1);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Settling);
}

TEST_F(LatencyTunerTest, RequestResetRebaselinesNonZeroXRunCountAndClearsDynamicFloor) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(2);
    tuner.setSettleCount(2);
    tuner.setCallbacksBeforeStepDown(4);

    // Drain Idle.
    tuner.tune();
    tuner.tune();

    // Bump twice to 4 bursts.
    mStream.addXRuns(1);
    tuner.tune();
    tuner.tune();
    tuner.tune();
    mStream.addXRuns(1);
    tuner.tune();
    tuner.tune();
    tuner.tune();
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);

    // Step down to 3 bursts after 4 quiet callbacks, then immediately glitch to raise dynamic floor.
    for (int i = 0; i < 4; ++i) {
        tuner.tune();
    }
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    mStream.addXRuns(1);
    tuner.tune();
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);
    ASSERT_EQ(tuner.getEffectiveMinimumBufferSize(), 4 * kBurst);

    // Now requestReset() while stream xRunCount is non-zero (3).
    tuner.requestReset();

    // First tune() after requestReset() processes the reset and enters Idle.
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Idle);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getEffectiveMinimumBufferSize(), 2 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 0);
    EXPECT_EQ(tuner.getStepDownCount(), 0);
    EXPECT_EQ(tuner.getSuppressedXRunCount(), 0);

    // Complete Idle window.
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // Next Active callback with unchanged xRunCount (still 3) MUST NOT bump!
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getBumpUpCount(), 0);
}

TEST_F(LatencyTunerTest, StepDownAfterQuietWindowReducesBufferSizeFromAtMaxToMin) {
    LatencyTuner tuner(mStream, 4 * kBurst); // max = 4 bursts, min = 2 bursts
    tuner.setIdleCount(1);
    tuner.setSettleCount(1);
    tuner.setCallbacksBeforeStepDown(5);

    tuner.tune(); // Idle -> Active

    // Bump twice from 2 bursts -> 4 bursts (AtMax).
    mStream.addXRuns(1);
    tuner.tune(); // bump to 3 bursts -> Settling
    tuner.tune(); // Settling -> Active
    mStream.addXRuns(1);
    tuner.tune(); // bump to 4 bursts -> AtMax
    ASSERT_EQ(tuner.getState(), LatencyTuner::State::AtMax);
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);

    // 5 glitch-free callbacks in AtMax should step down to 3 bursts (Active).
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(tuner.tune(), Result::OK);
        EXPECT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);
    }
    EXPECT_EQ(tuner.tune(), Result::OK);
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    EXPECT_EQ(tuner.getStepDownCount(), 1);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // Another 5 glitch-free callbacks step down to 2 bursts (minimumBufferSize).
    for (int i = 0; i < 5; ++i) {
        tuner.tune();
    }
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getStepDownCount(), 2);

    // Once at minimumBufferSize, further quiet callbacks do not step down below minimum.
    for (int i = 0; i < 20; ++i) {
        tuner.tune();
    }
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getStepDownCount(), 2);
}

TEST_F(LatencyTunerTest, StepDownBackoffHysteresisPreventsSawtoothOscillation) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(1);
    tuner.setSettleCount(1);
    tuner.setCallbacksBeforeStepDown(10);
    tuner.setStepDownBackoffEnabled(true);

    tuner.tune(); // Idle -> Active

    // Bump from 2 -> 3 -> 4 bursts.
    mStream.addXRuns(1);
    tuner.tune();
    tuner.tune();
    mStream.addXRuns(1);
    tuner.tune();
    tuner.tune();
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);

    // After 10 quiet callbacks, tuner probes 3 bursts and is immediately Active.
    for (int i = 0; i < 10; ++i) {
        tuner.tune();
    }
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    ASSERT_EQ(tuner.getStepDownCount(), 1);
    ASSERT_EQ(tuner.getState(), LatencyTuner::State::Active);

    // Suppose 3 bursts is unstable for this workload: an xRun occurs on the 3rd callback (< 10).
    tuner.tune();
    tuner.tune();
    mStream.addXRuns(1);
    tuner.tune();

    // Buffer size bumps back to 4 bursts AND effective minimum floor locks at 4 bursts!
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);
    EXPECT_EQ(tuner.getEffectiveMinimumBufferSize(), 4 * kBurst);

    // Even after 100 quiet callbacks, tuner never steps down below 4 bursts again.
    for (int i = 0; i < 100; ++i) {
        tuner.tune();
    }
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);
    EXPECT_EQ(tuner.getStepDownCount(), 1);
}

TEST_F(LatencyTunerTest, UnsupportedStreamTransitionsToUnsupportedState) {
    FakeAudioStream unsupportedStream(kBurst, kInitialSize, kCapacity);
    unsupportedStream.setXRunSupported(false);

    LatencyTuner tuner(unsupportedStream);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Unsupported);
    EXPECT_EQ(tuner.tune(), Result::ErrorUnimplemented);
}

TEST_F(LatencyTunerTest, RealisticWorkloadSimulationComparison) {
    // Compare 3 configurations over 5,000 callbacks on a workload whose true stability floor
    // is 4 bursts (384 frames), with:
    // - cold-start jitter during callbacks 0..4 (2 xRuns)
    // - multi-callback underrun episodes (each glitch increments xRunCount for 3 callbacks in a row)
    //   whenever bufferSize < 4 bursts (every 25 callbacks)
    // - one isolated heavy GC/CPU spike at callback 500 that causes a 4-callback xRun burst even at 4 bursts.
    auto runSimulation = [](int32_t settleCount, int32_t stepDownCallbacks, bool backoff) {
        FakeAudioStream stream(kBurst, 2 * kBurst, 8 * kBurst);
        LatencyTuner tuner(stream);
        tuner.setIdleCount(8);
        tuner.setSettleCount(settleCount);
        tuner.setCallbacksBeforeStepDown(stepDownCallbacks);
        tuner.setStepDownBackoffEnabled(backoff);

        int32_t remainingBurstXRuns = 0;
        for (int cb = 0; cb < 5000; ++cb) {
            if (cb == 1 || cb == 3) {
                // Cold-start xRuns during startup window
                stream.addXRuns(1);
            } else if (cb == 500) {
                // One-off system-wide CPU spike (4 consecutive callbacks report xRun increments)
                remainingBurstXRuns = 4;
            } else if (cb >= 8 && stream.getBufferSizeInFrames() < 4 * kBurst && (cb % 25 == 0)) {
                // Workload underruns whenever buffer size is below 4 bursts
                remainingBurstXRuns = 3;
            }

            if (remainingBurstXRuns > 0) {
                stream.addXRuns(1);
                remainingBurstXRuns--;
            }
            tuner.tune();
        }
        return std::make_tuple(
                stream.getBufferSizeInFrames(),
                tuner.getBumpUpCount(),
                tuner.getStepDownCount());
    };

    // 1. Legacy mode (settle=0, no step-down): a single 3-callback glitch bumps 3 times,
    //    and the spike at cb=500 pins the buffer permanently at max capacity (8 bursts = 768 frames).
    auto [legacySize, legacyBumps, legacyStepDowns] = runSimulation(0, 0, false);
    EXPECT_EQ(legacySize, 8 * kBurst);
    EXPECT_GE(legacyBumps, 6);
    EXPECT_EQ(legacyStepDowns, 0);

    // 2. Debounced mode (settle=8, no step-down): each multi-callback glitch bumps only 1 burst.
    //    Reaches 4 bursts before cb=500, and the single spike at cb=500 bumps it only once to 5 bursts.
    auto [debouncedSize, debouncedBumps, debouncedStepDowns] = runSimulation(8, 0, false);
    EXPECT_EQ(debouncedSize, 5 * kBurst);
    EXPECT_EQ(debouncedBumps, 3);
    EXPECT_EQ(debouncedStepDowns, 0);

    // 3. Debounced + StepDown + Backoff (settle=8, stepDown=200, backoff=true):
    //    After the one-off spike at cb=500 bumps to 5 bursts, it steps back down to 4 bursts after
    //    200 quiet callbacks, probes 3 bursts once, detects the underrun at 3 bursts, locks the
    //    dynamic floor at 4 bursts, and remains rock-solid at 4 bursts for the remaining 4,000+ callbacks!
    auto [adaptiveSize, adaptiveBumps, adaptiveStepDowns] = runSimulation(8, 200, true);
    EXPECT_EQ(adaptiveSize, 4 * kBurst);
    EXPECT_LE(adaptiveBumps, 5);
    EXPECT_LE(adaptiveStepDowns, 3);
}

TEST_F(LatencyTunerTest, StepDownBackoffDisabledAllowsRepeatedStepDown) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(1);
    tuner.setSettleCount(1);
    tuner.setCallbacksBeforeStepDown(5);
    tuner.setStepDownBackoffEnabled(false);

    tuner.tune(); // Idle -> Active

    // Bump from 2 -> 3 bursts.
    mStream.addXRuns(1);
    tuner.tune();
    tuner.tune();
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);

    // Step down to 2 bursts after 5 quiet callbacks.
    for (int i = 0; i < 5; ++i) {
        tuner.tune();
    }
    ASSERT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);

    // Trigger xRun right after step-down -> bumps back to 3 bursts, but effective minimum stays 2 bursts.
    mStream.addXRuns(1);
    tuner.tune();
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    EXPECT_EQ(tuner.getEffectiveMinimumBufferSize(), 2 * kBurst);

    // Finish 1 settling callback, then 5 quiet callbacks step down to 2 bursts again.
    tuner.tune();
    for (int i = 0; i < 5; ++i) {
        tuner.tune();
    }
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 2 * kBurst);
    EXPECT_EQ(tuner.getStepDownCount(), 2);
}

TEST_F(LatencyTunerTest, CustomConstructorAndSettersClampAndAlignProperly) {
    mStream.setQuantizeToBurst(true);
    LatencyTuner tuner(mStream, 5 * kBurst);

    EXPECT_EQ(tuner.getMaximumBufferSize(), 5 * kBurst);
    tuner.setMinimumBufferSize(3 * kBurst);
    EXPECT_EQ(tuner.getMinimumBufferSize(), 3 * kBurst);
    EXPECT_EQ(tuner.getEffectiveMinimumBufferSize(), 3 * kBurst);

    // Negative / invalid setters clamp safely.
    tuner.setIdleCount(-10);
    EXPECT_EQ(tuner.getIdleCount(), 0);
    tuner.setSettleCount(-5);
    EXPECT_EQ(tuner.getSettleCount(), 0);
    tuner.setXRunThreshold(0);
    EXPECT_EQ(tuner.getXRunThreshold(), 1);
    tuner.setCallbacksBeforeStepDown(-100);
    EXPECT_EQ(tuner.getCallbacksBeforeStepDown(), 0);

    // Non-burst increment is quantized by HAL and reflected accurately.
    tuner.setBufferSizeIncrement(50);
    tuner.requestReset();
    tuner.tune(); // IdleCount == 0 -> immediately Active at 3 * kBurst
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);

    mStream.addXRuns(1);
    tuner.tune();
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 4 * kBurst);

    // Lowering minimum buffer size to 1 burst (96) must immediately lower effective minimum.
    tuner.setMinimumBufferSize(1 * kBurst);
    EXPECT_EQ(tuner.getMinimumBufferSize(), 1 * kBurst);
    EXPECT_EQ(tuner.getEffectiveMinimumBufferSize(), 1 * kBurst);

    // Sub-burst decrement (50 < 96) with burst-quantizing stream still steps down by 1 burst.
    tuner.setBufferSizeDecrement(50);
    tuner.setCallbacksBeforeStepDown(2);
    tuner.tune();
    tuner.tune();
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 3 * kBurst);
    EXPECT_EQ(tuner.getStepDownCount(), 1);

    // Raising setMaximumBufferSize while AtMax transitions back to Active and allows bumping.
    tuner.setCallbacksBeforeStepDown(0);
    mStream.addXRuns(1);
    tuner.tune(); // 3 -> 4 bursts
    mStream.addXRuns(1);
    tuner.tune(); // 4 -> 5 bursts (AtMax)
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::AtMax);
    tuner.setMaximumBufferSize(7 * kBurst);
    EXPECT_EQ(tuner.getState(), LatencyTuner::State::Active);
    mStream.addXRuns(1);
    tuner.tune();
    EXPECT_EQ(mStream.getBufferSizeInFrames(), 6 * kBurst);
}

TEST_F(LatencyTunerTest,ConcurrentRequestResetFromMultipleThreads) {
    LatencyTuner tuner(mStream);
    tuner.setIdleCount(2);
    tuner.setSettleCount(2);
    tuner.setCallbacksBeforeStepDown(10);

    std::atomic<bool> running{true};
    std::thread resetter([&]() {
        for (int i = 0; i < 500 && running.load(); ++i) {
            tuner.requestReset();
        }
    });

    for (int cb = 0; cb < 5000; ++cb) {
        if (cb % 7 == 0) {
            mStream.addXRuns(1);
        }
        EXPECT_EQ(tuner.tune(), Result::OK);
        int32_t bufSize = mStream.getBufferSizeInFrames();
        EXPECT_GE(bufSize, tuner.getMinimumBufferSize());
        EXPECT_LE(bufSize, tuner.getMaximumBufferSize());
    }

    running.store(false);
    resetter.join();
}

