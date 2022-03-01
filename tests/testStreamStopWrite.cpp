/*
 * Copyright 2022 The Android Open Source Project
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

class TestStreamStopWrite : public ::testing::Test {

protected:

    void SetUp(){
        mBuilder.setPerformanceMode(PerformanceMode::None);
        mBuilder.setDirection(Direction::Output);
    }

    bool openStream(Direction direction, PerformanceMode perfMode) {
        mBuilder.setDirection(direction);
        mBuilder.setPerformanceMode(perfMode);
        Result r = mBuilder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        if (r != Result::OK)
            return false;

        Direction d = mStream->getDirection();
        EXPECT_EQ(d, direction) << convertToText(mStream->getDirection());
        return (d == direction);
    }

    bool openStream(AudioStreamBuilder &builder) {
        Result r = builder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    bool closeStream() {
        Result r = mStream->close();
        EXPECT_TRUE(r == Result::OK || r == Result::ErrorClosed) <<
            "Failed to close stream. " << convertToText(r);
        return (r == Result::OK || r == Result::ErrorClosed);
    }

    void stopWhileWritingLargeBuffer() {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        AudioStream *str = mStream;
        std::thread stopper([str] {
            usleep(2 * 1000);
            str->requestStop();
        });

        int16_t buffer[100000] = {};
        r = mStream->write(&buffer, 100000, kTimeoutInNanos);
        if (r != Result::OK) {
            FAIL() << "Could not write to audio stream";
        }

        r = mStream->waitForStateChange(StreamState::Started, &next,
                                        1000 * kNanosPerMillisecond);
        stopper.join();
        EXPECT_EQ(r, Result::OK);
        // May have caught in stopping transition. Wait for full stop.
        if (next == StreamState::Stopping) {
            r = mStream->waitForStateChange(StreamState::Stopping, &next,
                                            1000 * kNanosPerMillisecond);
            EXPECT_EQ(r, Result::OK);
        }
        ASSERT_EQ(next, StreamState::Stopped) << "next = " << convertToText(next);
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;
    static constexpr int kTimeoutInNanos = 1000 * kNanosPerMillisecond;

};

TEST_F(TestStreamStopWrite, OutputLowLatency) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    stopWhileWritingLargeBuffer();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamStopWrite, OutputNone) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::None));
    stopWhileWritingLargeBuffer();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamStopWrite, OutputPowerSavings) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::PowerSaving));
    stopWhileWritingLargeBuffer();
    ASSERT_TRUE(closeStream());
}