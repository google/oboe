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

using namespace oboe;

// Regression and stress tests for AAudio stream lifecycle & AAudioStreamCollection
// when streams are destroyed or overwritten without calling close() first.
// See https://github.com/google/oboe/issues/2444
class TestStreamLifecycle : public ::testing::Test {
protected:
    void SetUp() override {
        mWasMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    }

    void TearDown() override {
        AAudioExtensions::getInstance().setMMapEnabled(mWasMMapEnabled);
        // Allow any detached callback-thread AAudioStream cleanup tasks to finish.
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
                // reset() now waits for this callback, so keep the hold short.
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
                // reset() now waits for this callback, so keep the hold short.
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

            // Allow time for any trailing AudioTrack / MMAP callbacks and async close.
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

// A stream owned by a static object is destroyed during exit(), after function-local statics.
// The destructor now closes unclosed streams, which touches the registry and AAudioLoader; an
// earlier revision of this fix aborted here with "pthread_mutex_lock called on a destroyed mutex".
class TestStreamLifecycle_ExitCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *audioStream, void *audioData,
                                    int32_t numFrames) override {
        std::memset(audioData, 0, static_cast<size_t>(numFrames) *
                                  static_cast<size_t>(audioStream->getBytesPerFrame()));
        return DataCallbackResult::Continue;
    }
};

static void openStaticStreamAndExit(bool closeFirst) {
    // Callback first, so it is destroyed after the stream.
    static auto sCallback = std::make_shared<TestStreamLifecycle_ExitCallback>();
    static std::shared_ptr<AudioStream> sStream;
    AudioStreamBuilder builder;
    builder.setAudioApi(AudioApi::AAudio)
           ->setDirection(Direction::Output)
           ->setPerformanceMode(PerformanceMode::LowLatency)
           ->setUsage(Usage::Game)
           ->setDataCallback(sCallback);
    if (builder.openStream(sStream) != Result::OK) std::_Exit(3);
    sStream->requestStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (closeFirst) sStream->close();
    std::exit(0);
}

TEST_F(TestStreamLifecycle, StaticStreamDestroyedDuringExit) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(openStaticStreamAndExit(/*closeFirst=*/false), ::testing::ExitedWithCode(0), "");
    EXPECT_EXIT(openStaticStreamAndExit(/*closeFirst=*/true), ::testing::ExitedWithCode(0), "");
}

// Closing a stream from its own data callback is not allowed. Oboe must reject it like AAudio
// does (ErrorInvalidState) without unregistering or leaking the stream, so a later close() from
// the app thread still works.
TEST_F(TestStreamLifecycle, CloseFromOwnDataCallbackIsRejected) {
    if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";

    class CloseInCallback : public AudioStreamDataCallback {
    public:
        DataCallbackResult onAudioReady(AudioStream *s, void *data, int32_t n) override {
            std::memset(data, 0, static_cast<size_t>(n) *
                                 static_cast<size_t>(s->getBytesPerFrame()));
            if (!attempted.exchange(true)) {
                closeResult.store(static_cast<int32_t>(s->close()));
                done.store(true);
            }
            return DataCallbackResult::Continue;
        }
        std::atomic<bool> attempted{false};
        std::atomic<bool> done{false};
        std::atomic<int32_t> closeResult{0};
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
        // The stream must still be open and owned by the app.
        EXPECT_NE(stream->getState(), StreamState::Closed);
        EXPECT_EQ(stream->close(), Result::OK);
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
// varying channel counts, formats, and FilterAudioStream wrappers so freed stream
// chunks are aggressively reused across threads while trailing callbacks fire.
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
