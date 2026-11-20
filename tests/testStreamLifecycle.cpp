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
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

#include "../src/aaudio/AAudioExtensions.h"
#include "../src/aaudio/AudioStreamAAudio.h"
#include "../src/common/FilterAudioStream.h"

using namespace oboe;

namespace {

void fillSilence(AudioStream *s, void *data, int32_t n) {
    if (data != nullptr && n > 0) {
        const int32_t bpf = s->getBytesPerFrame();
        if (bpf > 0) {
            std::memset(data, 0, static_cast<size_t>(n) * static_cast<size_t>(bpf));
        }
    }
}

template <typename Pred>
bool waitUntil(Pred &&pred, int32_t timeoutMs = 1500, int32_t pollUs = 500) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::microseconds(pollUs));
    }
    return pred();
}

// Waits until no callback is running and the count stays unchanged for 200 ms.
bool waitForCallbacksToStop(const std::atomic<int32_t> &counter,
                            const std::atomic<int32_t> &inCallback,
                            int32_t timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const int32_t before = counter.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (inCallback.load() == 0 && counter.load() == before) return true;
    }
    return false;
}

Result openRaw(AudioStreamBuilder &builder, AudioStream **out) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return builder.openStream(out);
#pragma clang diagnostic pop
}

class FnDataCallback : public AudioStreamDataCallback {
public:
    using Fn = std::function<DataCallbackResult(AudioStream *, void *, int32_t)>;
    explicit FnDataCallback(Fn fn = {}) : mFn(std::move(fn)) {}

    DataCallbackResult onAudioReady(AudioStream *s, void *data, int32_t n) override {
        inCallback.fetch_add(1);
        EXPECT_NE(s, nullptr);
        fillSilence(s, data, n);
        (void) s->getDeviceId(); // exercise virtual dispatch
        const int32_t c = count.fetch_add(1) + 1;
        DataCallbackResult r = DataCallbackResult::Continue;
        if (mFn) {
            r = mFn(s, data, n);
        } else if (holdOnCount.load() == c) {
            enteredHold.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        if (returnStop.load()) r = DataCallbackResult::Stop;
        inCallback.fetch_sub(1);
        return r;
    }

    std::atomic<int32_t> inCallback{0};
    std::atomic<int32_t> count{0};
    std::atomic<int32_t> holdOnCount{0};
    std::atomic<bool> enteredHold{false};
    std::atomic<bool> returnStop{false};

private:
    Fn mFn;
};

class FnPartialCallback : public AudioStreamPartialDataCallback {
public:
    using Fn = std::function<int32_t(AudioStream *, void *, int32_t)>;
    explicit FnPartialCallback(Fn fn = {}) : mFn(std::move(fn)) {}

    int32_t onPartialAudioReady(AudioStream *s, void *data, int32_t n) override {
        inCallback.fetch_add(1);
        EXPECT_NE(s, nullptr);
        fillSilence(s, data, n);
        (void) s->getDeviceId();
        const int32_t c = count.fetch_add(1) + 1;
        int32_t r = n;
        if (mFn) {
            r = mFn(s, data, n);
        } else if (holdOnCount.load() == c) {
            enteredHold.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        inCallback.fetch_sub(1);
        return r;
    }

    std::atomic<int32_t> inCallback{0};
    std::atomic<int32_t> count{0};
    std::atomic<int32_t> holdOnCount{0};
    std::atomic<bool> enteredHold{false};

private:
    Fn mFn;
};

class TestableAAudioStream : public AudioStreamAAudio {
public:
    using AudioStreamAAudio::AudioStreamAAudio;

    static void simulateDisconnect(TestableAAudioStream *s) {
        internalErrorCallback(static_cast<AAudioStream *>(s->getUnderlyingStream()),
                              s->getCallbackUserDataForTest(),
                              static_cast<aaudio_result_t>(Result::ErrorDisconnected));
    }

    static void simulateRoutingChange(TestableAAudioStream *s) {
        const int32_t deviceIds[] = {1};
        internalRoutingChangedCallback(static_cast<AAudioStream *>(s->getUnderlyingStream()),
                                       s->getCallbackUserDataForTest(), deviceIds, 1);
    }

    void registerSharedPtr(std::shared_ptr<AudioStream> self) {
        setWeakThis(self);
        AudioStreamAAudio::onWeakThisSet(this, self);
    }
};

class SlowErrorCallback : public AudioStreamErrorCallback {
public:
    void onErrorBeforeClose(AudioStream *, Result) override {
        enteredBeforeClose.store(true);
        if (holdMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    }
    void onErrorAfterClose(AudioStream *s, Result) override {
        sampleRateSeen.store(s->getSampleRate());
        (void) s->getState();
        if (deleteInAfterClose) delete s;
        afterCloseDone.store(true);
    }
    int holdMs = 250;
    bool deleteInAfterClose = false;
    std::atomic<bool> enteredBeforeClose{false};
    std::atomic<bool> afterCloseDone{false};
    std::atomic<int32_t> sampleRateSeen{0};
};

// Pre-main global stream destroyed after libaaudio's own static destructors unless Oboe's atexit
// handler closes it first.
static auto gExitCallback = std::make_shared<FnDataCallback>();
static std::shared_ptr<AudioStream> gExitStream;

AudioStreamBuilder makeOutputBuilder(bool mmap, bool convert,
                                     std::shared_ptr<AudioStreamDataCallback> cb,
                                     int32_t framesPerCallback = 0) {
    AudioStreamBuilder builder;
    builder.setAudioApi(AudioApi::AAudio)
           ->setDirection(Direction::Output)
           ->setPerformanceMode(PerformanceMode::LowLatency)
           ->setSharingMode(SharingMode::Shared)
           ->setUsage(mmap ? Usage::Game : Usage::Notification)
           ->setDataCallback(std::move(cb));
    if (framesPerCallback > 0) {
        builder.setFramesPerDataCallback(framesPerCallback);
    }
    if (convert) {
        builder.setFormat(AudioFormat::I16)
               ->setChannelCount(1)
               ->setSampleRate(11025)
               ->setFormatConversionAllowed(true)
               ->setChannelConversionAllowed(true)
               ->setSampleRateConversionQuality(SampleRateConversionQuality::Fastest);
    }
    return builder;
}

bool openStartedExitStream(std::shared_ptr<AudioStream> &stream,
                           const std::shared_ptr<AudioStreamDataCallback> &cb) {
    AudioStreamBuilder builder = makeOutputBuilder(/*mmap=*/true, /*convert=*/false, cb);
    if (builder.openStream(stream) != Result::OK) return false;
    if (stream->requestStart() != Result::OK) {
        stream->close();
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

void runFunctionStaticExit(bool closeFirst) {
    static auto sCb = std::make_shared<FnDataCallback>();
    static std::shared_ptr<AudioStream> sStream;
    if (!openStartedExitStream(sStream, sCb)) std::_Exit(3);
    if (closeFirst) sStream->close();
    std::exit(0);
}

void runGlobalExit(bool closeFirst) {
    if (!openStartedExitStream(gExitStream, gExitCallback)) std::_Exit(3);
    if (closeFirst) gExitStream->close();
    std::exit(0);
}

std::unique_ptr<FilterAudioStream> openFilterPair(
        AudioStreamBuilder &builder,
        int32_t delayMs,
        std::shared_ptr<TestableAAudioStream> *childOut) {
    auto child = std::make_shared<TestableAAudioStream>(builder);
    if (child->open() != Result::OK) return nullptr;
    child->setDelayBeforeCloseMillis(delayMs);
    AudioStreamBuilder parentBuilder = builder;
    parentBuilder.setFormat(AudioFormat::I16)
                 ->setSampleRate(child->getSampleRate())
                 ->setChannelCount(child->getChannelCount());
    auto parent = std::make_unique<FilterAudioStream>(parentBuilder, child);
    child->registerSharedPtr(child);
    if (parent->configureFlowGraph() != Result::OK || parent->requestStart() != Result::OK) {
        return nullptr;
    }
    if (childOut) *childOut = std::move(child);
    return parent;
}

} // namespace

// Regression and stress tests for AAudio stream lifecycle (https://github.com/google/oboe/issues/2444).
class TestStreamLifecycle : public ::testing::Test {
protected:
    void SetUp() override {
        if (!AudioStreamBuilder::isAAudioSupported()) GTEST_SKIP() << "AAudio not supported";
        mWasMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
        mSavedMMap = true;
    }

    void TearDown() override {
        if (!mSavedMMap) return;
        AAudioExtensions::getInstance().setMMapEnabled(mWasMMapEnabled);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    template <typename Fn>
    void runWithDeadlockTimeout(int timeoutSeconds, Fn &&fn) {
        std::packaged_task<void()> task(std::forward<Fn>(fn));
        auto future = task.get_future();
        std::thread worker(std::move(task));
        if (future.wait_for(std::chrono::seconds(timeoutSeconds)) == std::future_status::timeout) {
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
    bool mSavedMMap = false;
};

// Repro of #2444 across Legacy/MMAP and direct/FilterAudioStream: dropping the app's last
// shared_ptr without close(), both while onAudioReady() is in-flight and between callbacks.
TEST_F(TestStreamLifecycle, ResetUnclosedSharedStream) {
    runWithDeadlockTimeout(120, []() {
        for (bool mmap : {false, true}) {
            if (mmap && !AAudioExtensions::getInstance().isMMapSupported()) continue;
            AAudioExtensions::getInstance().setMMapEnabled(mmap);
            for (bool convert : {false, true}) {
                SCOPED_TRACE(testing::Message() << "mmap=" << mmap << " convert=" << convert);
                for (int i = 0; i < 20; ++i) {
                    const bool hold = (i % 2 == 0);
                    auto cb = std::make_shared<FnDataCallback>();
                    cb->holdOnCount.store(hold ? 2 : 0);
                    auto builder = makeOutputBuilder(mmap, convert, cb);
                    std::shared_ptr<AudioStream> stream;
                    ASSERT_EQ(builder.openStream(stream), Result::OK) << "cycle " << i;
                    stream->setDelayBeforeCloseMillis(0);
                    ASSERT_EQ(stream->requestStart(), Result::OK) << "cycle " << i;
                    if (hold) {
                        ASSERT_TRUE(waitUntil([&]() { return cb->enteredHold.load(); })) << i;
                        stream.reset();
                        EXPECT_TRUE(waitForCallbacksToStop(cb->count, cb->inCallback));
                    } else {
                        ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; })) << i;
                        stream.reset();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
    });
}

// Overwriting an already-open shared_ptr in builder.openStream() and deleting an unclosed raw
// stream pointer, alternating direct AudioStreamAAudio and FilterAudioStream.
TEST_F(TestStreamLifecycle, OverwriteSharedAndDeleteRawUnclosedStreams) {
    runWithDeadlockTimeout(45, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        std::shared_ptr<AudioStream> shared;
        for (int i = 0; i < 25; ++i) {
            auto cb = std::make_shared<FnDataCallback>();
            auto builder = makeOutputBuilder(/*mmap=*/false, /*convert=*/(i % 2 == 1), cb);
            ASSERT_EQ(builder.openStream(shared), Result::OK) << "overwrite " << i;
            shared->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(shared->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));
        }
        if (shared) EXPECT_EQ(shared->close(), Result::OK);
        shared.reset();

        for (int i = 0; i < 16; ++i) {
            auto cb = std::make_shared<FnDataCallback>();
            auto builder = makeOutputBuilder(/*mmap=*/false, /*convert=*/(i % 2 == 1), cb);
            AudioStream *raw = nullptr;
            ASSERT_EQ(openRaw(builder, &raw), Result::OK) << "raw " << i;
            ASSERT_NE(raw, nullptr);
            raw->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(raw->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));
            delete raw; // intentionally unclosed
        }
    });
}

// Partial data callbacks: unclosed stream reset during/between callbacks, plus control calls
// (requestPause/Flush/Start/close/requestStop) invoked from onPartialAudioReady().
TEST_F(TestStreamLifecycle, PartialDataCallbackLifecycleAndControlCalls) {
    if (!OboeExtensions::isPartialDataCallbackSupported()) {
        GTEST_SKIP() << "Partial data callback not supported";
    }
    runWithDeadlockTimeout(45, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 14; ++i) {
            const bool hold = (i % 2 == 0);
            auto cb = std::make_shared<FnPartialCallback>();
            cb->holdOnCount.store(hold ? 2 : 0);
            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::None)
                   ->setUsage(Usage::Notification)
                   ->setPartialDataCallback(cb);
            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(builder.openStream(stream), Result::OK) << "cycle " << i;
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            if (hold) {
                ASSERT_TRUE(waitUntil([&]() { return cb->enteredHold.load(); }));
                stream.reset();
                EXPECT_TRUE(waitForCallbacksToStop(cb->count, cb->inCallback));
            } else {
                ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));
                stream.reset();
            }
        }

        for (bool callClose : {false, true}) {
            std::atomic<bool> attempted{false}, done{false};
            std::atomic<int32_t> pauseR{-1}, flushR{-1}, startR{-1}, actionR{-1};
            auto cb = std::make_shared<FnPartialCallback>(
                    [&](AudioStream *s, void *, int32_t n) -> int32_t {
                        if (!attempted.exchange(true)) {
                            pauseR.store(static_cast<int32_t>(s->requestPause()));
                            flushR.store(static_cast<int32_t>(s->requestFlush()));
                            startR.store(static_cast<int32_t>(s->requestStart()));
                            actionR.store(static_cast<int32_t>(
                                    callClose ? s->close() : s->requestStop()));
                            done.store(true);
                        }
                        return n;
                    });
            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::None)
                   ->setUsage(Usage::Notification)
                   ->setPartialDataCallback(cb);
            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(builder.openStream(stream), Result::OK);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return done.load(); }, 2000));
            EXPECT_EQ(static_cast<Result>(pauseR.load()), Result::ErrorInvalidState);
            EXPECT_EQ(static_cast<Result>(flushR.load()), Result::ErrorInvalidState);
            EXPECT_EQ(static_cast<Result>(startR.load()), Result::ErrorInvalidState);
            EXPECT_EQ(static_cast<Result>(actionR.load()),
                      callClose ? Result::ErrorInvalidState : Result::OK);
            EXPECT_TRUE(waitForCallbacksToStop(cb->count, cb->inCallback));
            EXPECT_EQ(stream->close(), Result::OK);
        }
    });
}

TEST_F(TestStreamLifecycle, ResetUnclosedInputStream) {
    AAudioExtensions::getInstance().setMMapEnabled(false);
    {
        AudioStreamBuilder probe;
        probe.setAudioApi(AudioApi::AAudio)->setDirection(Direction::Input);
        std::shared_ptr<AudioStream> probeStream;
        Result r = probe.openStream(probeStream);
        if (r != Result::OK) GTEST_SKIP() << "Input open failed: " << convertToText(r);
        probeStream->close();
    }
    runWithDeadlockTimeout(30, []() {
        for (int i = 0; i < 12; ++i) {
            const bool hold = (i % 2 == 0);
            auto cb = std::make_shared<FnDataCallback>();
            cb->holdOnCount.store(hold ? 2 : 0);
            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Input)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setFramesPerDataCallback(64)
                   ->setDataCallback(cb);
            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(builder.openStream(stream), Result::OK) << "cycle " << i;
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            if (hold) {
                ASSERT_TRUE(waitUntil([&]() { return cb->enteredHold.load(); }));
                stream.reset();
                EXPECT_TRUE(waitForCallbacksToStop(cb->count, cb->inCallback));
            } else {
                ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));
                stream.reset();
            }
        }
    });
}

// Static/global stream cleanup during exit() and fork() safety (child exit must not touch parent).
TEST_F(TestStreamLifecycle, ExitAndForkSafety) {
    {
        std::shared_ptr<AudioStream> probe;
        if (!openStartedExitStream(probe, std::make_shared<FnDataCallback>())) {
            GTEST_SKIP() << "Cannot open output stream";
        }
        probe->close();
    }
    const std::string oldStyle = ::testing::GTEST_FLAG(death_test_style);
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    for (bool closeFirst : {false, true}) {
        EXPECT_EXIT(runFunctionStaticExit(closeFirst), ::testing::ExitedWithCode(0), "")
                << "function static, close=" << closeFirst;
        EXPECT_EXIT(runGlobalExit(closeFirst), ::testing::ExitedWithCode(0), "")
                << "global, close=" << closeFirst;
    }
    ::testing::GTEST_FLAG(death_test_style) = oldStyle;

    runWithDeadlockTimeout(30, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto cb = std::make_shared<FnDataCallback>();
        std::shared_ptr<AudioStream> stream;
        ASSERT_EQ(makeOutputBuilder(false, false, cb).openStream(stream), Result::OK);
        stream->setDelayBeforeCloseMillis(0);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));

        fflush(nullptr);
        const pid_t child = fork();
        ASSERT_GE(child, 0);
        if (child == 0) exit(0);

        int status = 0;
        pid_t waited = 0;
        waitUntil([&]() { return (waited = waitpid(child, &status, WNOHANG)) != 0; }, 10000, 10000);
        if (waited == 0) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            FAIL() << "forked child hung in exit()";
        }
        EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        const int32_t before = cb->count.load();
        EXPECT_TRUE(waitUntil([&]() { return cb->count.load() >= before + 2; }));
        EXPECT_EQ(stream->getState(), StreamState::Started);
        EXPECT_EQ(stream->close(), Result::OK);
    });
}

// close()/release() from own data callback return ErrorInvalidState, stop audio, and allow restart.
TEST_F(TestStreamLifecycle, CloseAndReleaseFromOwnDataCallbackRejected) {
    runWithDeadlockTimeout(30, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (bool convert : {false, true})
        for (bool useRelease : {false, true}) {
            if (useRelease && (convert || getSdkVersion() < __ANDROID_API_S__)) continue;
            SCOPED_TRACE(testing::Message() << "convert=" << convert << " release=" << useRelease);
            std::atomic<bool> attempted{false}, done{false};
            std::atomic<int32_t> res{0};
            auto cb = std::make_shared<FnDataCallback>([&](AudioStream *s, void *, int32_t) {
                if (!attempted.exchange(true)) {
                    res.store(static_cast<int32_t>(useRelease ? s->release() : s->close()));
                    done.store(true);
                }
                return DataCallbackResult::Continue;
            });
            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(makeOutputBuilder(false, convert, cb).openStream(stream), Result::OK);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return done.load(); }, 2000));
            EXPECT_EQ(static_cast<Result>(res.load()), Result::ErrorInvalidState);
            EXPECT_TRUE(waitForCallbacksToStop(cb->count, cb->inCallback));
            EXPECT_NE(stream->getState(), StreamState::Closed);

            const int32_t stoppedCount = cb->count.load();
            ASSERT_EQ(stream->requestStart(), Result::OK);
            EXPECT_TRUE(waitUntil([&]() { return cb->count.load() >= stoppedCount + 5; }, 2000));
            EXPECT_EQ(stream->close(), Result::OK);
        }
    });
}

// requestStop()/stop() and requestPause()/Flush()/Start() from own data callback never deadlock
// (including concurrently with app close()), and match AAudio's API-level return codes.
TEST_F(TestStreamLifecycle, StopAndControlCallsFromOwnDataCallback) {
    runWithDeadlockTimeout(45, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (bool convert : {false, true})
        for (bool blockingStop : {false, true}) {
            SCOPED_TRACE(testing::Message() << "convert=" << convert << " stop=" << blockingStop);
            for (bool raceWithClose : {false, true}) {
                std::atomic<bool> trigger{false}, entered{false}, done{false};
                std::atomic<int32_t> stopRes{-1}, stopUs{0};
                auto cb = std::make_shared<FnDataCallback>([&](AudioStream *s, void *, int32_t) {
                    if (trigger.load() && !entered.exchange(true)) {
                        if (raceWithClose) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(40));
                        }
                        const auto t0 = std::chrono::steady_clock::now();
                        const Result r = blockingStop ? s->stop() : s->requestStop();
                        stopUs.store(static_cast<int32_t>(
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - t0).count()));
                        stopRes.store(static_cast<int32_t>(r));
                        done.store(true);
                    }
                    return DataCallbackResult::Continue;
                });
                std::shared_ptr<AudioStream> stream;
                ASSERT_EQ(makeOutputBuilder(false, convert, cb).openStream(stream), Result::OK);
                stream->setDelayBeforeCloseMillis(0);
                ASSERT_EQ(stream->requestStart(), Result::OK);
                ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 3; }));
                trigger.store(true);
                if (raceWithClose) {
                    ASSERT_TRUE(waitUntil([&]() { return entered.load(); }, 2000));
                    EXPECT_EQ(stream->close(), Result::OK);
                    ASSERT_TRUE(waitUntil([&]() { return done.load(); }, 2000));
                } else {
                    ASSERT_TRUE(waitUntil([&]() { return done.load(); }, 2000));
                    EXPECT_LT(stopUs.load(), 20000);
                    EXPECT_TRUE(waitForCallbacksToStop(cb->count, cb->inCallback));
                    EXPECT_EQ(stream->close(), Result::OK);
                }
                EXPECT_EQ(static_cast<Result>(stopRes.load()), Result::OK);
            }
        }

        // Control calls during concurrent close() across Legacy/MMAP and direct/FilterAudioStream.
        for (bool mmap : {false, true}) {
            if (mmap && !AAudioExtensions::getInstance().isMMapSupported()) continue;
            AAudioExtensions::getInstance().setMMapEnabled(mmap);
            for (bool convert : {false, true}) {
                std::atomic<bool> entered{false}, closeStarted{false}, done{false};
                std::atomic<int32_t> unexpected{0};
                auto cb = std::make_shared<FnDataCallback>([&](AudioStream *s, void *, int32_t) {
                    if (!entered.exchange(true)) {
                        waitUntil([&]() { return closeStarted.load(); }, 500, 200);
                        waitUntil([&]() { return s->getState() == StreamState::Closed; }, 500, 200);
                        const Result expected = (getSdkVersion() >= __ANDROID_API_P__)
                                ? Result::ErrorInvalidState : Result::ErrorClosed;
                        for (Result r : {s->requestPause(), s->requestFlush(), s->requestStart()}) {
                            if (r != expected) unexpected.fetch_add(1);
                        }
                        done.store(true);
                    }
                    return DataCallbackResult::Continue;
                });
                std::shared_ptr<AudioStream> stream;
                ASSERT_EQ(makeOutputBuilder(mmap, convert, cb).openStream(stream), Result::OK);
                stream->setDelayBeforeCloseMillis(0);
                ASSERT_EQ(stream->requestStart(), Result::OK);
                ASSERT_TRUE(waitUntil([&]() { return entered.load(); }));
                closeStarted.store(true);
                EXPECT_EQ(stream->close(), Result::OK);
                EXPECT_TRUE(done.load());
                EXPECT_EQ(unexpected.load(), 0);
            }
        }
    });
}

// Raw-pointer streams (direct and FilterAudioStream) wait for active error/routing helper threads
// on delete, unless deleted from inside onErrorAfterClose() on that helper thread itself.
TEST_F(TestStreamLifecycle, RawStreamDeleteHelperThreadLifetime) {
    runWithDeadlockTimeout(40, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);

        // 1. Direct raw stream deleted during onErrorBeforeClose() (including >500 ms warning).
        for (int holdMs : {600, 80}) {
            auto data = std::make_shared<FnDataCallback>();
            auto err = std::make_shared<SlowErrorCallback>();
            err->holdMs = holdMs;
            auto builder = makeOutputBuilder(false, false, data);
            builder.setErrorCallback(err);
            auto stream = std::make_unique<TestableAAudioStream>(builder);
            ASSERT_EQ(stream->open(), Result::OK);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return data->count.load() >= 2; }));

            TestableAAudioStream::simulateDisconnect(stream.get());
            ASSERT_TRUE(waitUntil([&]() { return err->enteredBeforeClose.load(); }, 2000));
            stream.reset();
            EXPECT_TRUE(err->afterCloseDone.load());
            EXPECT_GT(err->sampleRateSeen.load(), 0);
        }

        // 2. Direct raw stream deleted during onRoutingChanged().
        {
            class SlowRoutingCb : public AudioStreamRoutingCallback {
            public:
                void onRoutingChanged(AudioStream *s, const int32_t *, int32_t) override {
                    entered.store(true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    sampleRateSeen.store(s->getSampleRate());
                    done.store(true);
                }
                std::atomic<bool> entered{false}, done{false};
                std::atomic<int32_t> sampleRateSeen{0};
            };
            auto data = std::make_shared<FnDataCallback>();
            auto routing = std::make_shared<SlowRoutingCb>();
            auto builder = makeOutputBuilder(false, false, data);
            builder.setRoutingCallback(routing);
            auto stream = std::make_unique<TestableAAudioStream>(builder);
            ASSERT_EQ(stream->open(), Result::OK);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return data->count.load() >= 2; }));

            TestableAAudioStream::simulateRoutingChange(stream.get());
            ASSERT_TRUE(waitUntil([&]() { return routing->entered.load(); }, 2000));
            stream.reset();
            EXPECT_TRUE(routing->done.load());
            EXPECT_GT(routing->sampleRateSeen.load(), 0);
        }

        // 3. Raw stream deleted inside onErrorAfterClose() does not wait for itself.
        {
            auto data = std::make_shared<FnDataCallback>();
            auto err = std::make_shared<SlowErrorCallback>();
            err->holdMs = 0;
            err->deleteInAfterClose = true;
            auto builder = makeOutputBuilder(false, false, data);
            builder.setErrorCallback(err);
            auto stream = std::make_unique<TestableAAudioStream>(builder);
            ASSERT_EQ(stream->open(), Result::OK);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return data->count.load() >= 2; }));

            const auto t0 = std::chrono::steady_clock::now();
            TestableAAudioStream::simulateDisconnect(stream.release());
            ASSERT_TRUE(waitUntil([&]() { return err->afterCloseDone.load(); }, 5000));
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
            EXPECT_LT(ms, 1500);
        }

        // 4. Raw FilterAudioStream deleted while child's error helper thread calls into parent.
        {
            auto data = std::make_shared<FnDataCallback>();
            auto err = std::make_shared<SlowErrorCallback>();
            auto builder = makeOutputBuilder(false, false, data);
            builder.setErrorCallback(err);
            std::shared_ptr<TestableAAudioStream> child;
            auto parent = openFilterPair(builder, /*delayMs=*/0, &child);
            ASSERT_NE(parent, nullptr);
            ASSERT_TRUE(waitUntil([&]() { return data->count.load() >= 2; }));

            TestableAAudioStream::simulateDisconnect(child.get());
            child.reset();
            ASSERT_TRUE(waitUntil([&]() { return err->enteredBeforeClose.load(); }, 2000));
            parent.reset();
            EXPECT_TRUE(err->afterCloseDone.load());
            EXPECT_GT(err->sampleRateSeen.load(), 0);
        }
    });
}

// Reopening a closed shared_ptr stream restores registry state for data and routing callbacks.
TEST_F(TestStreamLifecycle, ReopenClosedSharedStreamDeliversDataAndRoutingCallbacks) {
    runWithDeadlockTimeout(20, []() {
        class CountingRoutingCb : public AudioStreamRoutingCallback {
        public:
            void onRoutingChanged(AudioStream *s, const int32_t *, int32_t) override {
                (void) s->getSampleRate();
                count.fetch_add(1);
            }
            std::atomic<int32_t> count{0};
        };
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto data = std::make_shared<FnDataCallback>();
        auto routing = std::make_shared<CountingRoutingCb>();
        auto builder = makeOutputBuilder(false, false, data);
        builder.setRoutingCallback(routing);
        auto stream = std::make_shared<TestableAAudioStream>(builder);
        ASSERT_EQ(stream->open(), Result::OK);
        stream->registerSharedPtr(stream);
        stream->setDelayBeforeCloseMillis(0);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        ASSERT_TRUE(waitUntil([&]() { return data->count.load() >= 2; }));
        ASSERT_EQ(stream->close(), Result::OK);

        data->count.store(0);
        ASSERT_EQ(stream->open(), Result::OK);
        ASSERT_EQ(stream->requestStart(), Result::OK);
        EXPECT_TRUE(waitUntil([&]() { return data->count.load() >= 3; }));
        TestableAAudioStream::simulateRoutingChange(stream.get());
        EXPECT_TRUE(waitUntil([&]() { return routing->count.load() >= 1; }, 2000));
        EXPECT_EQ(stream->close(), Result::OK);
    });
}

// Exercises DataCallbackResult::Stop racing with unclosed reset() (including API <= R stop thread)
// and unclosed reset() with error, routing, and presentation callbacks attached.
TEST_F(TestStreamLifecycle, ConcurrentStopAndAuxCallbacksUnclosedReset) {
    runWithDeadlockTimeout(40, []() {
        class AuxErr : public AudioStreamErrorCallback {
            void onErrorAfterClose(AudioStream *s, Result) override { if (s) (void) s->getState(); }
        };
        class AuxRoute : public AudioStreamRoutingCallback {
            void onRoutingChanged(AudioStream *s, const int32_t *, int32_t) override {
                if (s) (void) s->getDeviceIds();
            }
        };
        class AuxPres : public AudioStreamPresentationCallback {
            void onPresentationEnded(AudioStream *s) override { if (s) (void) s->getState(); }
        };
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto errCb = std::make_shared<AuxErr>();
        auto routeCb = std::make_shared<AuxRoute>();
        auto presCb = std::make_shared<AuxPres>();

        for (int i = 0; i < 20; ++i) {
            auto cb = std::make_shared<FnDataCallback>();
            cb->holdOnCount.store(1);
            cb->returnStop.store(i % 2 == 0);
            auto builder = makeOutputBuilder(false, (i % 3 == 0), cb);
            builder.setErrorCallback(errCb)->setRoutingCallback(routeCb)
                   ->setPresentationCallback(presCb);
            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(builder.openStream(stream), Result::OK) << "cycle " << i;
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return cb->enteredHold.load(); })) << i;
            stream.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

// Multi-threaded ABA address-reuse and registry-slot churn across direct & FilterAudioStream.
TEST_F(TestStreamLifecycle, MultiThreadedConcurrentChurnABA) {
    runWithDeadlockTimeout(35, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        constexpr int kThreads = 3, kCycles = 20;
        std::atomic<int> opens{0};
        std::vector<std::thread> workers;
        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([t, &opens]() {
                for (int i = 0; i < kCycles; ++i) {
                    const bool explicitClose = ((t + i) % 4 == 3);
                    const bool hold = ((t + i) % 2 == 0) && !explicitClose;
                    auto cb = std::make_shared<FnDataCallback>();
                    cb->holdOnCount.store(hold ? 2 : 0);
                    auto builder = makeOutputBuilder(false, ((t + i) % 3 == 1), cb);
                    builder.setFramesPerDataCallback(32);
                    std::shared_ptr<AudioStream> stream;
                    if (builder.openStream(stream) != Result::OK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    opens.fetch_add(1);
                    stream->setDelayBeforeCloseMillis(0);
                    if (stream->requestStart() == Result::OK) {
                        (void) stream->getState();
                        (void) stream->getDeviceId();
                        if (hold) {
                            waitUntil([&]() { return cb->enteredHold.load(); }, 300);
                            stream.reset();
                        } else {
                            waitUntil([&]() { return cb->count.load() >= 2; }, 300);
                            if (explicitClose) stream->close();
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
        for (auto &w : workers) w.join();
        EXPECT_GT(opens.load(), kThreads * (kCycles / 2));
    });
}

// No app data callback may still be running or start after close() returns, even with delay=0.
TEST_F(TestStreamLifecycle, NoDataCallbackAfterCloseReturns) {
    class LateCallback : public AudioStreamDataCallback {
    public:
        DataCallbackResult onAudioReady(AudioStream *s, void *data, int32_t n) override {
            inCallback.fetch_add(1);
            if (closeReturned.load()) {
                lateCalls.fetch_add(1); // never touch `s`: stream may already be freed
                inCallback.fetch_sub(1);
                return DataCallbackResult::Stop;
            }
            fillSilence(s, data, n);
            if (seen.fetch_add(1) + 1 == 3) {
                entered.store(true);
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
            inCallback.fetch_sub(1);
            return DataCallbackResult::Continue;
        }
        std::atomic<int32_t> inCallback{0}, seen{0}, lateCalls{0};
        std::atomic<bool> entered{false}, closeReturned{false};
    };

    runWithDeadlockTimeout(60, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        for (int i = 0; i < 24; ++i) {
            auto cb = std::make_shared<LateCallback>();
            std::shared_ptr<AudioStream> stream;
            ASSERT_EQ(makeOutputBuilder(false, (i % 2 == 1), cb).openStream(stream), Result::OK);
            stream->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(stream->requestStart(), Result::OK);
            ASSERT_TRUE(waitUntil([&]() { return cb->entered.load(); })) << i;

            EXPECT_EQ(stream->close(), Result::OK) << i;
            EXPECT_EQ(cb->inCallback.load(), 0) << "callback still running after close()";
            cb->closeReturned.store(true);
            stream.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            EXPECT_EQ(cb->lateCalls.load(), 0) << "callback started after close()";
        }
    });
}

// Open/close, unclosed destructor, blocking-stream, and failed-open cycles must reuse registry
// slots in place without leaking slots or growing the backing table.
TEST_F(TestStreamLifecycle, OpenCloseAndFailedOpensDoNotLeakSlots) {
    runWithDeadlockTimeout(90, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto runCycle = [](int i) {
            std::shared_ptr<AudioStream> stream;
            auto cb = (i % 5 == 0) ? nullptr : std::make_shared<FnDataCallback>();
            AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                   ->setDirection(Direction::Output)
                   ->setPerformanceMode(PerformanceMode::LowLatency)
                   ->setUsage(Usage::Notification);
            if (cb) builder.setDataCallback(cb);
            ASSERT_EQ(builder.openStream(stream), Result::OK) << "cycle " << i;
            stream->setDelayBeforeCloseMillis(0);
            if (cb && i % 10 == 1) {
                ASSERT_EQ(stream->requestStart(), Result::OK);
                ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));
            }
            if (i % 2 == 0) EXPECT_EQ(stream->close(), Result::OK);
            stream.reset();
        };
        runCycle(0); // warm up 1 slot so allocated table size stays flat
        const int32_t slotsBefore = AudioStreamAAudio::getSlotCountForTest();
        const int32_t allocatedBefore = AudioStreamAAudio::getAllocatedSlotCountForTest();
        for (int i = 0; i < 100; ++i) {
            runCycle(i);
            if (::testing::Test::HasFatalFailure()) return;
        }
        // Failed opens must release their acquired registry slot.
        for (int i = 0; i < 10; ++i) {
            AudioStreamBuilder bad;
            bad.setAudioApi(AudioApi::AAudio)->setSampleRate(9999999);
            std::shared_ptr<AudioStream> stream;
            (void) bad.openStream(stream);
        }
        EXPECT_EQ(AudioStreamAAudio::getSlotCountForTest(), slotsBefore);
        EXPECT_EQ(AudioStreamAAudio::getAllocatedSlotCountForTest(), allocatedBefore);
    });
}

// Releasing an already-stopped stream skips sleepBeforeClose(); releasing a running stream sleeps.
TEST_F(TestStreamLifecycle, DestructorSkipsCloseDelayForStoppedStream) {
    if (!OboeGlobals::areWorkaroundsEnabled()) GTEST_SKIP() << "close delay needs workarounds";
    runWithDeadlockTimeout(45, []() {
        constexpr int kDelayMs = 250;
        AAudioExtensions::getInstance().setMMapEnabled(false);
        auto timeDelete = [](bool stopFirst) -> long long {
            auto cb = std::make_shared<FnDataCallback>();
            auto builder = makeOutputBuilder(false, false, cb);
            AudioStream *raw = nullptr;
            if (openRaw(builder, &raw) != Result::OK || raw == nullptr) return -1;
            raw->setDelayBeforeCloseMillis(kDelayMs);
            bool ok = raw->requestStart() == Result::OK
                    && waitUntil([&]() { return cb->count.load() >= 2; });
            if (stopFirst) ok = ok && raw->stop() == Result::OK;
            ok = ok && raw->getState() == (stopFirst ? StreamState::Stopped : StreamState::Started);
            const auto t0 = std::chrono::steady_clock::now();
            delete raw;
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
            return ok ? ms : -1;
        };
        const long long stoppedMs = timeDelete(/*stopFirst=*/true);
        const long long runningMs = timeDelete(/*stopFirst=*/false);
        if (stoppedMs < 0 || runningMs < 0) GTEST_SKIP() << "stream did not reach expected state";
        EXPECT_GE(runningMs - stoppedMs, 180) << "stopped=" << stoppedMs << " running=" << runningMs;
    });
}

// Deleting a direct stream or FilterAudioStream while another thread is inside closeInternal()
// after nulling mAAudioStream must serialize on mCloseLock until closeInternal() finishes.
TEST_F(TestStreamLifecycle, DeleteDuringConcurrentCloseWaitsForCloseInternal) {
    if (!OboeGlobals::areWorkaroundsEnabled()) GTEST_SKIP() << "close delay needs workarounds";
    runWithDeadlockTimeout(30, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        constexpr int kDelayMs = 200;

        for (bool filter : {false, true}) {
            SCOPED_TRACE(testing::Message() << "filter=" << filter);
            auto cb = std::make_shared<FnDataCallback>();
            auto builder = makeOutputBuilder(false, false, cb);
            std::unique_ptr<AudioStream> toDelete;
            std::shared_ptr<TestableAAudioStream> child;
            if (!filter) {
                AudioStream *raw = nullptr;
                ASSERT_EQ(openRaw(builder, &raw), Result::OK);
                toDelete.reset(raw);
                toDelete->setDelayBeforeCloseMillis(kDelayMs);
                ASSERT_EQ(toDelete->requestStart(), Result::OK);
            } else {
                toDelete = openFilterPair(builder, kDelayMs, &child);
                ASSERT_NE(toDelete, nullptr);
            }
            ASSERT_TRUE(waitUntil([&]() { return cb->count.load() >= 2; }));

            std::atomic<int32_t> closeRes{-1};
            std::thread closer([&]() {
                closeRes.store(static_cast<int32_t>(filter ? child->close() : toDelete->close()));
            });
            const bool reachedClosed = waitUntil(
                    [&]() { return toDelete->getState() == StreamState::Closed; }, 2000, 200);
            EXPECT_TRUE(reachedClosed);

            const auto t0 = std::chrono::steady_clock::now();
            toDelete.reset();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
            closer.join();
            EXPECT_EQ(static_cast<Result>(closeRes.load()), Result::OK);
            if (reachedClosed) EXPECT_GE(ms, kDelayMs / 2);
        }
    });
}

// Unsupported self-destruction inside onAudioReady() must not UAF in callOnAudioReady() or allow
// acquire() to reuse the slot before leaveDataCallback() runs.
TEST_F(TestStreamLifecycle, SelfDestroyInsideOnAudioReadyDoesNotTouchFreedStream) {
    runWithDeadlockTimeout(30, []() {
        AAudioExtensions::getInstance().setMMapEnabled(false);
        const int32_t slotsBefore = AudioStreamAAudio::getSlotCountForTest();
        for (int i = 0; i < 5; ++i) {
            std::shared_ptr<AudioStream> holder;
            std::atomic<bool> allow{false}, attempted{false}, destroyed{false}, done{false};
            auto cb = std::make_shared<FnDataCallback>([&](AudioStream *, void *, int32_t) {
                if (allow.load(std::memory_order_acquire) && !attempted.exchange(true)) {
                    holder.reset();
                    destroyed.store(true, std::memory_order_release);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    done.store(true, std::memory_order_release);
                }
                return DataCallbackResult::Continue;
            });
            std::shared_ptr<AudioStream> localRef;
            ASSERT_EQ(makeOutputBuilder(false, false, cb).openStream(localRef), Result::OK) << i;
            localRef->setDelayBeforeCloseMillis(0);
            holder = localRef;
            const Result startR = localRef->requestStart();
            if (startR != Result::OK) {
                holder.reset();
                localRef->close();
                FAIL() << "requestStart failed: " << convertToText(startR);
            }
            localRef.reset();
            allow.store(true, std::memory_order_release);
            ASSERT_TRUE(waitUntil([&]() { return destroyed.load(std::memory_order_acquire); },
                                  2000));

            // Open/close another stream while the first callback is still unwinding (active == 1).
            auto otherCb = std::make_shared<FnDataCallback>();
            std::shared_ptr<AudioStream> other;
            ASSERT_EQ(makeOutputBuilder(false, false, otherCb).openStream(other), Result::OK);
            other->setDelayBeforeCloseMillis(0);
            ASSERT_EQ(other->close(), Result::OK);
            other.reset();

            ASSERT_TRUE(waitUntil([&]() { return done.load(std::memory_order_acquire); }, 2000));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        EXPECT_EQ(AudioStreamAAudio::getSlotCountForTest(), slotsBefore);
    });
}
