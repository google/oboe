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

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

#include "../src/aaudio/AAudioExtensions.h"
#include "../src/aaudio/AudioStreamAAudio.h"
#include "../src/common/FilterAudioStream.h"

using namespace oboe;

// Regression and stress tests for AAudio stream lifecycle: streams destroyed or overwritten
// without close(), stop/close called from the stream's own data callback, and raw-pointer
// streams deleted while Oboe helper threads are using them.
// See https://github.com/google/oboe/issues/2444
class TestStreamLifecycle : public ::testing::Test {
protected:
    void SetUp() override {
        mWasMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    }

    void TearDown() override {
        AAudioExtensions::getInstance().setMMapEnabled(mWasMMapEnabled);
        // Let detached Oboe threads (async close, helper threads) finish before the next test.
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    class LifecycleDataCallback : public AudioStreamDataCallback {
    public:
        DataCallbackResult onAudioReady(
                AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override {
            // Exercise virtual methods on audioStream to catch any UAF / vptr corruption.
            EXPECT_NE(audioStream, nullptr);
            int32_t bytesPerFrame = audioStream->getBytesPerFrame();
            (void) audioStream->getDeviceId();
            if (audioData != nullptr && numFrames > 0 && bytesPerFrame > 0) {
                std::memset(audioData, 0,
                            static_cast<size_t>(numFrames) * static_cast<size_t>(bytesPerFrame));
            }

            int32_t count = callbackCount.fetch_add(1) + 1;
            if (holdInCallback.load() && count >= targetHoldCount.load()) {
                enteredHold.store(true);
                // The stream destructor (via reset()) waits for this callback to return,
                // so keep the hold short.
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
                while (!releaseHold.load() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(300));
            }

            return returnStop.load() ? DataCallbackResult::Stop : DataCallbackResult::Continue;
        }

        std::atomic<int32_t> callbackCount{0};
        std::atomic<bool> holdInCallback{false};
        std::atomic<int32_t> targetHoldCount{2};
        std::atomic<bool> enteredHold{false};
        std::atomic<bool> releaseHold{false};
        std::atomic<bool> returnStop{false};
    };

    class LifecyclePartialDataCallback : public AudioStreamPartialDataCallback {
    public:
        int32_t onPartialAudioReady(
                AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override {
            EXPECT_NE(audioStream, nullptr);
            int32_t bytesPerFrame = audioStream->getBytesPerFrame();
            (void) audioStream->getDeviceId();
            if (audioData != nullptr && numFrames > 0 && bytesPerFrame > 0) {
                std::memset(audioData, 0,
                            static_cast<size_t>(numFrames) * static_cast<size_t>(bytesPerFrame));
            }

            int32_t count = callbackCount.fetch_add(1) + 1;
            if (holdInCallback.load() && count >= 2) {
                enteredHold.store(true);
                // The stream destructor (via reset()) waits for this callback to return,
                // so keep the hold short.
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
                while (!releaseHold.load() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            return numFrames;
        }

        std::atomic<int32_t> callbackCount{0};
        std::atomic<bool> holdInCallback{false};
        std::atomic<bool> enteredHold{false};
        std::atomic<bool> releaseHold{false};
    };

    static bool waitForCallbacks(const std::atomic<int32_t> &counter, int32_t minCount,
                                 int32_t timeoutMs = 1000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (counter.load() < minCount && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return counter.load() >= minCount;
    }

    static bool waitForFlag(const std::atomic<bool> &flag, int32_t timeoutMs = 1000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!flag.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        return flag.load();
    }

    void runUnclosedSharedStreamCycles(bool enableMMap, bool forceConversion, int numCycles) {
        AAudioExtensions::getInstance().setMMapEnabled(enableMMap);

        for (int i = 0; i < numCycles; ++i) {
            const bool resetDuringCallback = (i % 2 == 0);
            auto callback = std::make_shared<LifecycleDataCallback>();
            callback->holdInCallback.store(resetDuringCallback);
            callback->targetHoldCount.store(2);

            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setSharingMode(SharingMode::Shared)
                   // Game usage routes to the primary output, where MMAP is available.
                   ->setUsage(enableMMap ? Usage::Game : Usage::Notification)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(callback);

            if (forceConversion) {
                builder.setFormat(AudioFormat::I16)
                       ->setChannelCount(1)
                       ->setSampleRate(11025)
                       ->setFormatConversionAllowed(true)
                       ->setChannelConversionAllowed(true)
                       ->setSampleRateConversionQuality(SampleRateConversionQuality::Fastest);
            } else {
                builder.setFormat(AudioFormat::Float)
                       ->setChannelCount(2);
            }

            std::shared_ptr<AudioStream> stream;
            Result result = builder.openStream(stream);
            ASSERT_EQ(result, Result::OK) << "Cycle " << i << " failed openStream: "
                                          << convertToText(result);
            stream->setDelayBeforeCloseMillis(0);

            ASSERT_EQ(stream->requestStart(), Result::OK) << "Cycle " << i;

            if (resetDuringCallback) {
                ASSERT_TRUE(waitForFlag(callback->enteredHold))
                        << "Cycle " << i << " never entered callback hold";
                // Drop the app's only shared_ptr WHILE onAudioReady is running. The data
                // callback holds no reference (it is lock-free), so the destructor must
                // close the stream and wait for this in-flight callback before freeing it.
                stream.reset();
                callback->releaseHold.store(true);
            } else {
                ASSERT_TRUE(waitForCallbacks(callback->callbackCount, 2))
                        << "Cycle " << i << " did not fire callbacks";
                // Drop shared_ptr from the app thread without calling close().
                stream.reset();
            }

            // Brief pause between cycles so trailing callbacks and device teardown can settle.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    template <typename Fn>
    void runWithDeadlockTimeout(int timeoutSeconds, Fn &&fn) {
        // Not std::async: its future blocks in ~future() until the task finishes,
        // which would hang forever on a real deadlock instead of failing.
        std::packaged_task<void()> task(std::forward<Fn>(fn));
        auto future = task.get_future();
        std::thread worker(std::move(task));
        if (future.wait_for(std::chrono::seconds(timeoutSeconds)) ==
                std::future_status::timeout) {
            // A deadlocked worker may hold Oboe/AAudio locks and would corrupt later tests,
            // so fail loudly and stop the process.
            fprintf(stderr, "FATAL: %s timed out after %d s (potential deadlock)\n",
                    ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                    timeoutSeconds);
            fflush(stderr);
            std::abort();
        }
        worker.join();
        future.get();
    }

private:
    bool mWasMMapEnabled = true;
};

TEST_F(TestStreamLifecycle, ResetUnclosedSharedStreamLegacy) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
    runWithDeadlockTimeout(30, [this]() {
        // Leaked tracks from unclosed streams would eventually make openStream fail.
        runUnclosedSharedStreamCycles(/*enableMMap=*/false, /*forceConversion=*/false, 40);
    });
}

TEST_F(TestStreamLifecycle, ResetUnclosedSharedStreamMMap) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
    if (!AAudioExtensions::getInstance().isMMapSupported()) GTEST_SKIP() << "MMAP not supported";
    runWithDeadlockTimeout(30, [this]() {
        runUnclosedSharedStreamCycles(/*enableMMap=*/true, /*forceConversion=*/false, 40);
    });
}

// Closest reproduction of Issue #2444: a FilterAudioStream (Legacy path, so data flows through
// FixedBlockReader::processVariableBlock) destroyed without close() while callbacks are running.
TEST_F(TestStreamLifecycle, ResetUnclosedFilterAudioStreamLegacy) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
    runWithDeadlockTimeout(30, [this]() {
        runUnclosedSharedStreamCycles(/*enableMMap=*/false, /*forceConversion=*/true, 35);
    });
}

TEST_F(TestStreamLifecycle, ResetUnclosedFilterAudioStreamMMap) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
    if (!AAudioExtensions::getInstance().isMMapSupported()) GTEST_SKIP() << "MMAP not supported";
    runWithDeadlockTimeout(30, [this]() {
        runUnclosedSharedStreamCycles(/*enableMMap=*/true, /*forceConversion=*/true, 35);
    });
}

// Tests passing an already-open, actively-running shared_ptr<AudioStream> back into
// builder.openStream(stream) without calling stream->close() first.
TEST_F(TestStreamLifecycle, OverwriteUnclosedSharedStreamInBuilderOpenStream) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }

    runWithDeadlockTimeout(30, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        std::shared_ptr<AudioStream> stream;

        for (int i = 0; i < 40; ++i) {
            auto callback = std::make_shared<LifecycleDataCallback>();
            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setUsage(Usage::Notification)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(callback);

            if (i % 2 == 1) {
                // Alternate direct AudioStreamAAudio and FilterAudioStream on the same pointer.
                builder.setSampleRate(11025)
                       ->setChannelCount(1)
                       ->setChannelConversionAllowed(true)
                       ->setSampleRateConversionQuality(SampleRateConversionQuality::Fastest);
            }

            Result result = builder.openStream(stream);
            ASSERT_EQ(result, Result::OK) << "Iteration " << i << ": " << convertToText(result);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK) << "Iteration " << i;
            ASSERT_TRUE(waitForCallbacks(callback->callbackCount, 2)) << "Iteration " << i;
        }

        if (stream) {
            EXPECT_EQ(stream->close(), Result::OK);
        }
    });
}

// Tests deprecated raw-pointer openStream(AudioStream**) deleted without close(),
// for both direct AudioStreamAAudio and FilterAudioStream.
TEST_F(TestStreamLifecycle, DeleteUnclosedRawStreamPointer) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }

    runWithDeadlockTimeout(25, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 20; ++i) {
            auto callback = std::make_shared<LifecycleDataCallback>();
            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setUsage(Usage::Notification)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(callback);

            if (i % 2 == 1) {
                builder.setSampleRate(11025)
                       ->setChannelCount(1)
                       ->setChannelConversionAllowed(true)
                       ->setSampleRateConversionQuality(SampleRateConversionQuality::Fastest);
            }

            AudioStream *rawStream = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            Result result = builder.openStream(&rawStream);
#pragma clang diagnostic pop
            ASSERT_EQ(result, Result::OK) << "Iteration " << i;
            ASSERT_NE(rawStream, nullptr);
            rawStream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(rawStream->requestStart(), Result::OK);
            ASSERT_TRUE(waitForCallbacks(callback->callbackCount, 2));

            // Intentionally delete without calling close().
            delete rawStream;
        }
    });
}

TEST_F(TestStreamLifecycle, ResetUnclosedPartialDataCallbackStream) {
    if (!AudioStreamBuilder::isAAudioSupported() ||
        !OboeExtensions::isPartialDataCallbackSupported()) {
        GTEST_SKIP() << "Partial data callback not supported";
    }

    runWithDeadlockTimeout(25, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 20; ++i) {
            const bool resetDuringCallback = (i % 2 == 0);
            auto callback = std::make_shared<LifecyclePartialDataCallback>();
            callback->holdInCallback.store(resetDuringCallback);

            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::None)
                   ->setUsage(Usage::Notification)
                   ->setPartialDataCallback(callback);

            std::shared_ptr<AudioStream> stream;
            Result result = builder.openStream(stream);
            ASSERT_EQ(result, Result::OK) << "Iteration " << i << ": " << convertToText(result);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);

            if (resetDuringCallback) {
                ASSERT_TRUE(waitForFlag(callback->enteredHold)) << "Iteration " << i;
                stream.reset();
                callback->releaseHold.store(true);
            } else {
                ASSERT_TRUE(waitForCallbacks(callback->callbackCount, 2)) << "Iteration " << i;
                stream.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

TEST_F(TestStreamLifecycle, ResetUnclosedInputStream) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }
    AAudioExtensions::getInstance().setMMapEnabled(false);
    {
        // Probe once on the main thread so a missing mic permission skips the test, while a
        // failed open later in the loop is reported as a real failure.
        AudioStreamBuilder probe;
        probe.setAudioApi(AudioApi::AAudio)->setDirection(Direction::Input);
        std::shared_ptr<AudioStream> probeStream;
        Result probeResult = probe.openStream(probeStream);
        if (probeResult != Result::OK) {
            GTEST_SKIP() << "Input stream open failed (likely mic permission): "
                         << convertToText(probeResult);
        }
        probeStream->close();
    }

    runWithDeadlockTimeout(25, []() {
        for (int i = 0; i < 15; ++i) {
            const bool resetDuringCallback = (i % 2 == 0);
            auto callback = std::make_shared<LifecycleDataCallback>();
            callback->holdInCallback.store(resetDuringCallback);

            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Input)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(callback);

            std::shared_ptr<AudioStream> stream;
            Result result = builder.openStream(stream);
            ASSERT_EQ(result, Result::OK) << "Iteration " << i << ": " << convertToText(result);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);

            if (resetDuringCallback) {
                ASSERT_TRUE(waitForFlag(callback->enteredHold)) << "Iteration " << i;
                stream.reset();
                callback->releaseHold.store(true);
            } else {
                ASSERT_TRUE(waitForCallbacks(callback->callbackCount, 2)) << "Iteration " << i;
                stream.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

// A stream destroyed during exit() without close(). Its destructor closes it, which uses Oboe's
// stream registry and AAudioLoader singletons. If those singletons were destroyed first, this
// would abort with "pthread_mutex_lock called on a destroyed mutex".
class TestStreamLifecycle_ExitCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *audioStream, void *audioData,
                                    int32_t numFrames) override {
        std::memset(audioData, 0, static_cast<size_t>(numFrames) *
                                  static_cast<size_t>(audioStream->getBytesPerFrame()));
        return DataCallbackResult::Continue;
    }
};

// Namespace-scope globals are constructed before main(), i.e. before Oboe loads libaaudio, so
// they are destroyed after libaaudio's own static objects. This is the worst case for exit().
static auto gExitCallback = std::make_shared<TestStreamLifecycle_ExitCallback>();
static std::shared_ptr<AudioStream> gExitStream;

static bool openExitStream(std::shared_ptr<AudioStream> &stream,
                           const std::shared_ptr<AudioStreamDataCallback> &callback) {
    AudioStreamBuilder builder;
    builder.setAudioApi(AudioApi::AAudio)
           ->setDirection(Direction::Output)
           ->setPerformanceMode(PerformanceMode::LowLatency)
           ->setUsage(Usage::Game)
           ->setDataCallback(callback);
    if (builder.openStream(stream) != Result::OK) return false;
    stream->requestStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

// Function-local statics are destroyed by exit() in reverse order of construction, while Oboe
// and libaaudio are still loaded. Complements the namespace-scope case (gExitStream above).
static void openFunctionStaticStreamAndExit(bool closeFirst) {
    // Callback first, so it is destroyed after the stream.
    static auto sCallback = std::make_shared<TestStreamLifecycle_ExitCallback>();
    static std::shared_ptr<AudioStream> sStream;
    if (!openExitStream(sStream, sCallback)) std::_Exit(3);
    if (closeFirst) sStream->close();
    std::exit(0);
}

static void openGlobalStreamAndExit(bool closeFirst) {
    if (!openExitStream(gExitStream, gExitCallback)) std::_Exit(3);
    if (closeFirst) gExitStream->close();
    std::exit(0);
}

TEST_F(TestStreamLifecycle, StaticStreamDestroyedDuringExit) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    for (bool closeFirst : {false, true}) {
        EXPECT_EXIT(openFunctionStaticStreamAndExit(closeFirst),
                    ::testing::ExitedWithCode(0), "") << "function static, close=" << closeFirst;
        EXPECT_EXIT(openGlobalStreamAndExit(closeFirst),
                    ::testing::ExitedWithCode(0), "") << "global, close=" << closeFirst;
    }
}

// close() from the stream's own data callback must return ErrorInvalidState without closing or
// unregistering the stream, and the callback must then return Stop so the audio stops.
// Restarting must resume callbacks, and close() from the app thread must still work.
TEST_F(TestStreamLifecycle, CloseFromOwnDataCallbackIsRejected) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    class CloseInCallback : public AudioStreamDataCallback {
    public:
        DataCallbackResult onAudioReady(AudioStream *s, void *data, int32_t n) override {
            std::memset(data, 0, static_cast<size_t>(n) *
                                 static_cast<size_t>(s->getBytesPerFrame()));
            count.fetch_add(1);
            if (!attempted.exchange(true)) {
                closeResult.store(static_cast<int32_t>(s->close()));
                done.store(true);
            }
            return DataCallbackResult::Continue;
        }
        std::atomic<bool> attempted{false};
        std::atomic<bool> done{false};
        std::atomic<int32_t> closeResult{0};
        std::atomic<int32_t> count{0};
    };

    runWithDeadlockTimeout(15, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto callback = std::make_shared<CloseInCallback>();
        AudioStreamBuilder builder;
        builder.setAudioApi(AudioApi::AAudio)
               ->setDirection(Direction::Output)
               ->setPerformanceMode(PerformanceMode::LowLatency)
               ->setUsage(Usage::Notification)
               ->setDataCallback(callback);
        std::shared_ptr<AudioStream> stream;
        ASSERT_EQ(builder.openStream(stream), Result::OK);
        stream->setDelayBeforeCloseMillis(0);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        ASSERT_TRUE(waitForFlag(callback->done, 2000));
        EXPECT_EQ(static_cast<Result>(callback->closeResult.load()), Result::ErrorInvalidState);

        // The stream must still be open, and callbacks must stop.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        EXPECT_NE(stream->getState(), StreamState::Closed);
        const int32_t countAfterStop = callback->count.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        EXPECT_LE(callback->count.load(), countAfterStop + 1) << "callbacks did not stop";

        // Restarting clears the stop request and callbacks resume.
        ASSERT_EQ(stream->requestStart(), Result::OK);
        EXPECT_TRUE(waitForCallbacks(callback->count, countAfterStop + 5, 2000))
                << "callbacks did not resume after restart";
        EXPECT_EQ(stream->close(), Result::OK);
    });
}

// requestStop() and stop() from the data callback must return OK quickly and stop the stream.
// They must also not deadlock with close() on another thread, which holds the stream lock while
// waiting for the callback to return. Covered for a plain AAudio stream and for a
// FilterAudioStream (data conversion), which forwards these calls to its child stream.
TEST_F(TestStreamLifecycle, StopFromOwnDataCallback) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    class StopInCallback : public AudioStreamDataCallback {
    public:
        explicit StopInCallback(bool useBlockingStop) : mUseBlockingStop(useBlockingStop) {}
        DataCallbackResult onAudioReady(AudioStream *s, void *data, int32_t n) override {
            std::memset(data, 0, static_cast<size_t>(n) *
                                 static_cast<size_t>(s->getBytesPerFrame()));
            count.fetch_add(1);
            if (stopNow.load() && !attempted.exchange(true)) {
                // Give the app thread time to enter close() and take its lock first.
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs.load()));
                auto start = std::chrono::steady_clock::now();
                Result r = mUseBlockingStop ? s->stop() : s->requestStop();
                stopMicros.store(static_cast<int32_t>(
                        std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - start).count()));
                stopResult.store(static_cast<int32_t>(r));
                done.store(true);
            }
            return DataCallbackResult::Continue;
        }
        const bool mUseBlockingStop;
        std::atomic<bool> stopNow{false};
        std::atomic<bool> attempted{false};
        std::atomic<bool> done{false};
        std::atomic<int32_t> delayMs{0};
        std::atomic<int32_t> stopResult{-1};
        std::atomic<int32_t> stopMicros{0};
        std::atomic<int32_t> count{0};
    };

    auto openStarted = [](std::shared_ptr<AudioStream> &stream,
                          const std::shared_ptr<StopInCallback> &callback,
                          bool forceConversion) {
        AudioStreamBuilder builder;
        builder.setAudioApi(AudioApi::AAudio)
               ->setDirection(Direction::Output)
               ->setPerformanceMode(PerformanceMode::LowLatency)
               ->setUsage(Usage::Notification)
               ->setDataCallback(callback);
        if (forceConversion) {
            builder.setFormat(AudioFormat::I16)
                   ->setChannelCount(1)
                   ->setSampleRate(11025)
                   ->setFormatConversionAllowed(true)
                   ->setChannelConversionAllowed(true)
                   ->setSampleRateConversionQuality(SampleRateConversionQuality::Fastest);
        }
        ASSERT_EQ(builder.openStream(stream), Result::OK);
        stream->setDelayBeforeCloseMillis(0);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        ASSERT_TRUE(waitForCallbacks(callback->count, 3));
    };

    runWithDeadlockTimeout(20, [&]() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (bool forceConversion : {false, true})
        for (bool blocking : {false, true}) {
            SCOPED_TRACE(testing::Message() << "blocking=" << blocking
                                            << " conversion=" << forceConversion);
            // 1. Stop from the callback stops the audio, quickly.
            {
                auto callback = std::make_shared<StopInCallback>(blocking);
                std::shared_ptr<AudioStream> stream;
                openStarted(stream, callback, forceConversion);
                if (::testing::Test::HasFatalFailure()) return;
                callback->stopNow.store(true);
                ASSERT_TRUE(waitForFlag(callback->done, 2000)) << "blocking=" << blocking;
                EXPECT_EQ(static_cast<Result>(callback->stopResult.load()), Result::OK);
                EXPECT_LT(callback->stopMicros.load(), 5000) << "stop blocked the callback";
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                const int32_t countAfterStop = callback->count.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                EXPECT_LE(callback->count.load(), countAfterStop + 1)
                        << "callbacks did not stop, blocking=" << blocking;
                EXPECT_EQ(stream->close(), Result::OK);
            }
            // 2. Stop from the callback while the app thread is inside close().
            {
                auto callback = std::make_shared<StopInCallback>(blocking);
                callback->delayMs.store(50);
                std::shared_ptr<AudioStream> stream;
                openStarted(stream, callback, forceConversion);
                if (::testing::Test::HasFatalFailure()) return;
                callback->stopNow.store(true);
                ASSERT_TRUE(waitForFlag(callback->attempted, 2000));
                EXPECT_EQ(stream->close(), Result::OK) << "blocking=" << blocking;
                ASSERT_TRUE(waitForFlag(callback->done, 2000));
                EXPECT_EQ(static_cast<Result>(callback->stopResult.load()), Result::OK);
            }
        }
    });
}

// Streams opened with the raw-pointer API have no shared_ptr to keep them alive while Oboe's
// error/routing/presentation helper threads use them. Deleting such a stream must therefore wait
// (up to 2 s) for those threads. These tests simulate a disconnect by calling the AAudio error
// callback directly, which makes the timing deterministic.
namespace {

class TestableAAudioStream : public AudioStreamAAudio {
public:
    using AudioStreamAAudio::AudioStreamAAudio;

    // Does what AAudio does when the device disconnects: calls the stream's error callback.
    static void simulateDisconnect(TestableAAudioStream *stream) {
        internalErrorCallback(static_cast<AAudioStream *>(stream->getUnderlyingStream()), stream,
                              static_cast<aaudio_result_t>(Result::ErrorDisconnected));
    }

    // Does what AAudio does when the output device changes: calls the routing callback.
    static void simulateRoutingChange(TestableAAudioStream *stream) {
        const int32_t deviceIds[] = {1};
        internalRoutingChangedCallback(static_cast<AAudioStream *>(stream->getUnderlyingStream()),
                                       stream, deviceIds, 1);
    }

    // Does what AudioStreamBuilder does after wrapping a new stream in a shared_ptr.
    void registerSharedPtr(std::shared_ptr<AudioStream> self) {
        setWeakThis(self);
        AudioStreamAAudio::onWeakThisSet(this, self);
    }
};

class SilenceCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *s, void *data, int32_t n) override {
        std::memset(data, 0, static_cast<size_t>(n) * static_cast<size_t>(s->getBytesPerFrame()));
        count.fetch_add(1);
        return DataCallbackResult::Continue;
    }
    std::atomic<int32_t> count{0};
};

// Error callback that holds the helper thread in onErrorBeforeClose() and then uses the stream
// in onErrorAfterClose(), like a typical app that logs or queries the stream there.
class SlowErrorCallback : public AudioStreamErrorCallback {
public:
    void onErrorBeforeClose(AudioStream *, Result) override {
        enteredBeforeClose.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    }
    void onErrorAfterClose(AudioStream *stream, Result) override {
        // Reads the stream object; this would be a use-after-free if the stream had been freed.
        sampleRateSeen.store(stream->getSampleRate());
        (void) stream->getState();
        if (deleteInAfterClose) {
            delete stream;
        }
        afterCloseDone.store(true);
    }
    int holdMs = 300;
    bool deleteInAfterClose = false;
    std::atomic<bool> enteredBeforeClose{false};
    std::atomic<bool> afterCloseDone{false};
    std::atomic<int32_t> sampleRateSeen{0};
};

AudioStreamBuilder makeRawTestBuilder(const std::shared_ptr<AudioStreamDataCallback> &data,
                                      const std::shared_ptr<AudioStreamErrorCallback> &error) {
    AudioStreamBuilder builder;
    builder.setAudioApi(AudioApi::AAudio)
           ->setDirection(Direction::Output)
           ->setPerformanceMode(PerformanceMode::LowLatency)
           ->setUsage(Usage::Notification)
           ->setFormat(AudioFormat::Float)
           ->setChannelCount(2)
           ->setDataCallback(data)
           ->setErrorCallback(error);
    return builder;
}

} // namespace

// The app deletes a raw-pointer stream on its own thread while the error helper thread is still
// inside the app's error callback. The delete must wait until the helper thread is done with the
// stream, so onErrorAfterClose() never sees freed memory.
TEST_F(TestStreamLifecycle, DeleteRawStreamDuringErrorCallbackWaitsForHelperThread) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    runWithDeadlockTimeout(20, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 5; ++i) {
            auto data = std::make_shared<SilenceCallback>();
            auto error = std::make_shared<SlowErrorCallback>();
            // The first iteration holds longer than the 500 ms warning threshold.
            error->holdMs = (i == 0) ? 1000 : 20 * i;
            AudioStreamBuilder builder = makeRawTestBuilder(data, error);
            auto *stream = new TestableAAudioStream(builder);
            ASSERT_EQ(stream->open(), Result::OK) << "iteration " << i;
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitForCallbacks(data->count, 2));

            TestableAAudioStream::simulateDisconnect(stream);
            ASSERT_TRUE(waitForFlag(error->enteredBeforeClose, 2000)) << "iteration " << i;

            // Delete without close(), while the helper thread is in onErrorBeforeClose().
            delete stream;
            EXPECT_TRUE(error->afterCloseDone.load())
                    << "delete returned while the error helper thread still used the stream, "
                    << "iteration " << i;
            EXPECT_GT(error->sampleRateSeen.load(), 0);
        }
    });
}

// Same as above for the routing callback, which runs on its own helper thread while the
// stream is still open.
TEST_F(TestStreamLifecycle, DeleteRawStreamDuringRoutingCallbackWaitsForHelperThread) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    class SlowRoutingCallback final : public AudioStreamRoutingCallback {
    public:
        void onRoutingChanged(AudioStream *stream, const int32_t *, int32_t) override {
            entered.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            // Reads the stream object; a use-after-free if the stream had been freed.
            sampleRateSeen.store(stream->getSampleRate());
            done.store(true);
        }
        std::atomic<bool> entered{false};
        std::atomic<bool> done{false};
        std::atomic<int32_t> sampleRateSeen{0};
    };

    runWithDeadlockTimeout(20, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto data = std::make_shared<SilenceCallback>();
        auto error = std::make_shared<SlowErrorCallback>();
        auto routing = std::make_shared<SlowRoutingCallback>();
        AudioStreamBuilder builder = makeRawTestBuilder(data, error);
        builder.setRoutingCallback(routing);
        auto *stream = new TestableAAudioStream(builder);
        ASSERT_EQ(stream->open(), Result::OK);
        stream->setDelayBeforeCloseMillis(0);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        ASSERT_TRUE(waitForCallbacks(data->count, 2));

        TestableAAudioStream::simulateRoutingChange(stream);
        ASSERT_TRUE(waitForFlag(routing->entered, 2000));
        delete stream;
        EXPECT_TRUE(routing->done.load())
                << "delete returned while the routing helper thread still used the stream";
        EXPECT_GT(routing->sampleRateSeen.load(), 0);
    });
}

// The app deletes the stream from inside onErrorAfterClose(), which runs on the error helper
// thread itself. The destructor must not wait for its own thread (that would hang until the
// timeout).
TEST_F(TestStreamLifecycle, DeleteRawStreamInsideOnErrorAfterClose) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    runWithDeadlockTimeout(20, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto data = std::make_shared<SilenceCallback>();
        auto error = std::make_shared<SlowErrorCallback>();
        error->holdMs = 0;
        error->deleteInAfterClose = true;
        AudioStreamBuilder builder = makeRawTestBuilder(data, error);
        auto *stream = new TestableAAudioStream(builder);
        ASSERT_EQ(stream->open(), Result::OK);
        stream->setDelayBeforeCloseMillis(0);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        ASSERT_TRUE(waitForCallbacks(data->count, 2));

        const auto start = std::chrono::steady_clock::now();
        TestableAAudioStream::simulateDisconnect(stream);
        ASSERT_TRUE(waitForFlag(error->afterCloseDone, 5000));
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        EXPECT_LT(elapsedMs, 1500) << "the destructor waited for its own helper thread";
    });
}

// Same as above for a raw-pointer FilterAudioStream (data conversion). The child's error helper
// thread calls the app through the parent FilterAudioStream, so deleting the parent must also
// wait for it.
TEST_F(TestStreamLifecycle, DeleteRawFilterStreamDuringErrorCallbackWaitsForHelperThread) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    runWithDeadlockTimeout(20, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 3; ++i) {
            auto data = std::make_shared<SilenceCallback>();
            auto error = std::make_shared<SlowErrorCallback>();
            AudioStreamBuilder builder = makeRawTestBuilder(data, error);
            // Built the same way as AudioStreamBuilder::openStreamInternal() wraps a child.
            auto child = std::make_shared<TestableAAudioStream>(builder);
            ASSERT_EQ(child->open(), Result::OK);
            child->setDelayBeforeCloseMillis(0);
            AudioStreamBuilder parentBuilder = builder;
            parentBuilder.setFormat(AudioFormat::I16)
                         ->setSampleRate(child->getSampleRate())
                         ->setChannelCount(child->getChannelCount());
            auto *parent = new FilterAudioStream(parentBuilder, child);
            child->registerSharedPtr(child);
            ASSERT_EQ(parent->configureFlowGraph(), Result::OK);
            ASSERT_EQ(parent->requestStart(), Result::OK);
            ASSERT_TRUE(waitForCallbacks(data->count, 2));

            TestableAAudioStream::simulateDisconnect(child.get());
            child.reset();  // The parent now owns the child, as in openStreamInternal().
            ASSERT_TRUE(waitForFlag(error->enteredBeforeClose, 2000)) << "iteration " << i;

            delete parent;
            EXPECT_TRUE(error->afterCloseDone.load())
                    << "delete returned while the error helper thread still used the parent, "
                    << "iteration " << i;
            EXPECT_GT(error->sampleRateSeen.load(), 0);
        }
    });
}

// Verifies that if a closed shared_ptr<AudioStream> is re-opened via stream->open(),
// AAudioStreamCollection preserves its weak_ptr registration and callbacks still fire.
TEST_F(TestStreamLifecycle, ReopenClosedSharedStreamDeliversCallbacks) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }

    auto callback = std::make_shared<LifecycleDataCallback>();
    AudioStreamBuilder builder;
    builder.setAudioApi(AudioApi::AAudio)
           ->setDirection(Direction::Output)
           ->setPerformanceMode(PerformanceMode::LowLatency)
           ->setUsage(Usage::Notification)
           ->setDataCallback(callback);

    std::shared_ptr<AudioStream> stream;
    ASSERT_EQ(builder.openStream(stream), Result::OK);
    ASSERT_EQ(stream->requestStart(), Result::OK);
    ASSERT_TRUE(waitForCallbacks(callback->callbackCount, 3));
    ASSERT_EQ(stream->close(), Result::OK);

    callback->callbackCount.store(0);
    ASSERT_EQ(stream->open(), Result::OK);
    ASSERT_EQ(stream->requestStart(), Result::OK);
    EXPECT_TRUE(waitForCallbacks(callback->callbackCount, 3));
    EXPECT_EQ(stream->close(), Result::OK);
}

// Exercises DataCallbackResult::Stop racing with unclosed shared_ptr::reset().
TEST_F(TestStreamLifecycle, ConcurrentCallbackStopAndUnclosedReset) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }

    runWithDeadlockTimeout(25, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 30; ++i) {
            auto callback = std::make_shared<LifecycleDataCallback>();
            callback->holdInCallback.store(true);
            callback->returnStop.store(true);
            callback->targetHoldCount.store(1);

            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setUsage(Usage::Notification)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(callback);

            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(builder.openStream(stream), Result::OK) << "Iteration " << i;
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);

            ASSERT_TRUE(waitForFlag(callback->enteredHold)) << "Iteration " << i;
            stream.reset();
            callback->releaseHold.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

// Tests unclosed stream reset when error, routing, and presentation callbacks are also attached.
TEST_F(TestStreamLifecycle, AuxiliaryCallbacksUnclosedReset) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }

    class AuxErrorCallback final : public AudioStreamErrorCallback {
    public:
        void onErrorAfterClose(AudioStream *oboeStream, Result /*error*/) override {
            if (oboeStream != nullptr) {
                (void) oboeStream->getState();
            }
        }
    };
    class AuxRoutingCallback final : public AudioStreamRoutingCallback {
    public:
        void onRoutingChanged(AudioStream *oboeStream,
                              const int32_t * /*deviceIds*/,
                              int32_t /*numDevices*/) override {
            if (oboeStream != nullptr) {
                (void) oboeStream->getDeviceIds();
            }
        }
    };
    class AuxPresentationCallback final : public AudioStreamPresentationCallback {
    public:
        void onPresentationEnded(AudioStream *oboeStream) override {
            if (oboeStream != nullptr) {
                (void) oboeStream->getState();
            }
        }
    };

    runWithDeadlockTimeout(25, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto errCb = std::make_shared<AuxErrorCallback>();
        auto routeCb = std::make_shared<AuxRoutingCallback>();
        auto presCb = std::make_shared<AuxPresentationCallback>();

        for (int i = 0; i < 25; ++i) {
            const bool resetDuringCallback = (i % 2 == 0);
            auto dataCb = std::make_shared<LifecycleDataCallback>();
            dataCb->holdInCallback.store(resetDuringCallback);

            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setUsage(Usage::Notification)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(dataCb)
                   ->setErrorCallback(errCb)
                   ->setRoutingCallback(routeCb)
                   ->setPresentationCallback(presCb);

            if (i % 3 == 0) {
                builder.setSampleRate(11025)
                       ->setChannelCount(1)
                       ->setChannelConversionAllowed(true)
                       ->setSampleRateConversionQuality(SampleRateConversionQuality::Fastest);
            }

            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(builder.openStream(stream), Result::OK) << "Iteration " << i;
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);

            if (resetDuringCallback) {
                ASSERT_TRUE(waitForFlag(dataCb->enteredHold)) << "Iteration " << i;
                stream.reset();
                dataCb->releaseHold.store(true);
            } else {
                ASSERT_TRUE(waitForCallbacks(dataCb->callbackCount, 2)) << "Iteration " << i;
                stream.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

// Multi-threaded ABA heap-address reuse and concurrency hammer:
// Multiple threads simultaneously open, start, query, and drop/close streams with
// varying channel counts, formats, and FilterAudioStream wrappers, so freed stream memory
// is quickly reused across threads while trailing callbacks fire.
TEST_F(TestStreamLifecycle, MultiThreadedConcurrentChurnABA) {
    if (!AudioStreamBuilder::isAAudioSupported()) {
        GTEST_SKIP() << "AAudio not supported";
    }

    runWithDeadlockTimeout(30, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        constexpr int kNumThreads = 3;
        constexpr int kCyclesPerThread = 20;
        std::atomic<int> successfulOpens{0};

        std::vector<std::thread> workers;
        workers.reserve(kNumThreads);
        for (int t = 0; t < kNumThreads; ++t) {
            workers.emplace_back([t, &successfulOpens]() {
                for (int i = 0; i < kCyclesPerThread; ++i) {
                    const bool hold = ((t + i) % 2 == 0);
                    const bool explicitClose = ((t + i) % 4 == 3);
                    const bool convert = ((t + i) % 3 == 1);

                    auto callback = std::make_shared<LifecycleDataCallback>();
                    callback->holdInCallback.store(hold && !explicitClose);

                    AudioStreamBuilder builder;
                    builder.setAudioApi(AudioApi::AAudio)
                           ->setDirection(Direction::Output)
                           ->setPerformanceMode(PerformanceMode::LowLatency)
                           ->setUsage(Usage::Notification)
                           ->setFramesPerDataCallback(32)
                           ->setDataCallback(callback);

                    if (convert) {
                        builder.setFormat(AudioFormat::I16)
                               ->setChannelCount(1)
                               ->setSampleRate(16000)
                               ->setFormatConversionAllowed(true)
                               ->setChannelConversionAllowed(true)
                               ->setSampleRateConversionQuality(
                                       SampleRateConversionQuality::Fastest);
                    } else {
                        builder.setFormat(AudioFormat::Float)
                               ->setChannelCount(2);
                    }

                    std::shared_ptr<AudioStream> stream;
                    if (builder.openStream(stream) != Result::OK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    successfulOpens.fetch_add(1);
                    stream->setDelayBeforeCloseMillis(0);
                    if (stream->requestStart() == Result::OK) {
                        (void) stream->getState();
                        (void) stream->getDeviceId();
                        if (hold && !explicitClose) {
                            (void) waitForFlag(callback->enteredHold, 300);
                            stream.reset();
                            callback->releaseHold.store(true);
                        } else {
                            (void) waitForCallbacks(callback->callbackCount, 2, 300);
                            if (explicitClose) {
                                stream->close();
                            }
                            stream.reset();
                        }
                    } else {
                        stream->close();
                        stream.reset();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(3));
                }
            });
        }

        for (auto &w : workers) {
            w.join();
        }
        EXPECT_GT(successfulOpens.load(), kNumThreads * (kCyclesPerThread / 2));
    });
}
