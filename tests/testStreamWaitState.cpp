/*
 * Copyright 2019 The Android Open Source Project
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

#include <thread>

#include <gtest/gtest.h>

#include <oboe/Oboe.h>

using namespace oboe;

class TestStreamWaitState : public ::testing::Test {

protected:

    void SetUp(){
        mBuilder.setPerformanceMode(PerformanceMode::None);
        mBuilder.setDirection(Direction::Output);
    }

    bool openStream(Direction direction, PerformanceMode perfMode) {
        mBuilder.setDirection(direction);
        Result r = mBuilder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        EXPECT_EQ(mStream->getDirection(), direction) << convertToText(mStream->getDirection());
        return (r == Result::OK);
    }

    bool openStream(AudioStreamBuilder &builder) {
        Result r = builder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    void closeStream() {
        if (mStream != nullptr){
            Result r = mStream->close();
            mStream = nullptr;
            if (r != Result::OK && r != Result::ErrorClosed){
                FAIL() << "Failed to close stream. " << convertToText(r);
            }
        }
    }

    void checkWaitZeroTimeout() {
        StreamState next = StreamState::Unknown;
        int64_t timeout = 0; // don't wait for a state change
		Result result = mStream->waitForStateChange(mStream->getState(), &next, timeout);
		EXPECT_EQ(Result::ErrorTimeout, result);
	}

    void checkStopWhileWaiting() {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        AudioStream *str = mStream;
        std::thread stopper([str] {
            usleep(200 * 1000);
            str->requestStop();
        });

        r = mStream->waitForStateChange(StreamState::Started, &next, 1000 * kNanosPerMillisecond);
        stopper.join();
        EXPECT_EQ(r, Result::OK);
        // May have caught in stopping transition. Wait for full stop.
        if (next == StreamState::Stopping) {
            r = mStream->waitForStateChange(StreamState::Stopping, &next, 1000 * kNanosPerMillisecond);
            EXPECT_EQ(r, Result::OK);
        }
        ASSERT_EQ(next, StreamState::Stopped) << "next = " << convertToText(next);
    }

    void checkCloseWhileWaiting() {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        AudioStream *str = mStream;
        std::thread closer([str] {
            usleep(200 * 1000);
            str->close();
        });

        r = mStream->waitForStateChange(StreamState::Started, &next, 1000 * kNanosPerMillisecond);
        closer.join();
        // You might catch this at any point in stopping or closing.
        EXPECT_TRUE(r == Result::OK || r == Result::ErrorClosed) << "r = " << convertToText(r);
        ASSERT_TRUE(next == StreamState::Stopping
                    || next == StreamState::Stopped
                    || next == StreamState::Pausing
                    || next == StreamState::Paused
                    || next == StreamState::Closed) << "next = " << convertToText(next);
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;
    static constexpr int kTimeoutInNanos = 100 * kNanosPerMillisecond;

};

TEST_F(TestStreamWaitState, OutputLowWaitZero) {
    openStream(Direction::Output, PerformanceMode::LowLatency);
    checkWaitZeroTimeout();
    closeStream();
}

TEST_F(TestStreamWaitState, OutputNoneWaitZero) {
    openStream(Direction::Output, PerformanceMode::None);
    checkWaitZeroTimeout();
    closeStream();
}

TEST_F(TestStreamWaitState, OutputLowWaitZeroSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    openStream(builder);
    checkWaitZeroTimeout();
    closeStream();
}

TEST_F(TestStreamWaitState, OutputNoneWaitZeroSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::None);
    builder.setAudioApi(AudioApi::OpenSLES);
    openStream(builder);
    checkWaitZeroTimeout();
    closeStream();
}


TEST_F(TestStreamWaitState, OutputLowStopWhileWaiting) {
    openStream(Direction::Output, PerformanceMode::LowLatency);
    checkStopWhileWaiting();
    closeStream();
}

TEST_F(TestStreamWaitState, OutputNoneStopWhileWaiting) {
    openStream(Direction::Output, PerformanceMode::LowLatency);
    checkStopWhileWaiting();
    closeStream();
}


TEST_F(TestStreamWaitState, OutputLowStopWhileWaitingSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    openStream(builder);
    checkStopWhileWaiting();
    closeStream();
}


TEST_F(TestStreamWaitState, OutputLowCloseWhileWaiting) {
    openStream(Direction::Output, PerformanceMode::LowLatency);
    checkCloseWhileWaiting();
    closeStream();
}

TEST_F(TestStreamWaitState, OutputNoneCloseWhileWaiting) {
    openStream(Direction::Output, PerformanceMode::None);
    checkCloseWhileWaiting();
    closeStream();
}

TEST_F(TestStreamWaitState, InputLowCloseWhileWaiting) {
    openStream(Direction::Input, PerformanceMode::LowLatency);
    checkCloseWhileWaiting();
    closeStream();
}

TEST_F(TestStreamWaitState, InputNoneCloseWhileWaiting) {
    openStream(Direction::Input, PerformanceMode::None);
    checkCloseWhileWaiting();
    closeStream();
}

TEST_F(TestStreamWaitState, OutputNoneCloseWhileWaitingSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::None);
    builder.setAudioApi(AudioApi::OpenSLES);
    openStream(builder);
    checkCloseWhileWaiting();
    closeStream();
}


TEST_F(TestStreamWaitState, OutputLowCloseWhileWaitingSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    openStream(builder);
    checkCloseWhileWaiting();
    closeStream();
}