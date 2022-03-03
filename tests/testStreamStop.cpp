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

class TestStreamStop : public ::testing::Test {

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

    void stopWhileUsingLargeBuffer(bool shouldWrite) {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        AudioStream *str = mStream;

        int16_t buffer[kFramesToWrite * 4] = {};

        std::thread stopper([str] {
            usleep(3 * 1000); // 3 ms
            str->close();
        });

        if (shouldWrite) {
            r = mStream->write(&buffer, kFramesToWrite, kTimeoutInNanos);
        } else {
            r = mStream->read(&buffer, kFramesToWrite, kTimeoutInNanos);
        }
        if (r != Result::OK) {
            FAIL() << "Could not write to audio stream: " << static_cast<int>(r);
        }

        stopper.join();
        r = mStream->waitForStateChange(StreamState::Started, &next,
                                        1000 * kNanosPerMillisecond);
        if ((r != Result::ErrorClosed) && (r != Result::OK)) {
            FAIL() << "Wrong closed result type: " << static_cast<int>(r);
        }
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;
    static constexpr int kTimeoutInNanos = 1000 * kNanosPerMillisecond;
    static constexpr int kFramesToWrite = 10000;

};

TEST_F(TestStreamStop, OutputLowLatency) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    stopWhileUsingLargeBuffer(true /* write */);
}

TEST_F(TestStreamStop, OutputPowerSavings) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::PowerSaving));
    stopWhileUsingLargeBuffer(true /* write */);
}

TEST_F(TestStreamStop, OutputNone) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::None));
    stopWhileUsingLargeBuffer(true /* write */);
}

TEST_F(TestStreamStop, InputLowLatency) {
    ASSERT_TRUE(openStream(Direction::Input, PerformanceMode::LowLatency));
    stopWhileUsingLargeBuffer(false /* read */);
}

TEST_F(TestStreamStop, InputPowerSavings) {
    ASSERT_TRUE(openStream(Direction::Input, PerformanceMode::PowerSaving));
    stopWhileUsingLargeBuffer(false /* read */);
}

TEST_F(TestStreamStop, InputNone) {
    ASSERT_TRUE(openStream(Direction::Input, PerformanceMode::None));
    stopWhileUsingLargeBuffer(false /* read */);
}