/*
 * Copyright 2018 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

using namespace oboe;

class StreamStates : public ::testing::Test {

protected:

    void SetUp(){

    }

    bool openStream(){
        Result r = mBuilder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    bool openInputStream(){
        mBuilder.setDirection(Direction::Input);
        return openStream();
    }

    void closeStream(){
        Result r = mStream->close();
        if (r != Result::OK){
            FAIL() << "Failed to close stream. " << convertToText(r);
        }
    }

    void openAndCloseStream(){

        openStream();
        closeStream();
        ASSERT_EQ(mStream->getState(), StreamState::Closed) << "Stream state " << convertToText(mStream->getState());
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;
    static constexpr int kTimeoutInNanos = 100 * kNanosPerMillisecond;

};

TEST_F(StreamStates, OutputStreamStateIsOpenAfterOpening){
    openStream();
    StreamState next = StreamState::Unknown;
    Result r = mStream->waitForStateChange(StreamState::Uninitialized, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK) << convertToText(r);
    ASSERT_EQ(next, StreamState::Open) << convertToText(next);
    closeStream();
}

TEST_F(StreamStates, OutputStreamStateIsStartedAfterStarting){

    openStream();

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Started);

    closeStream();
}

TEST_F(StreamStates, OutputStreamStateIsPausedAfterPausing){

    openStream();

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);
    r = mStream->requestPause();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Pausing, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Paused);

    closeStream();
}


TEST_F(StreamStates, OutputStreamStateIsStoppedAfterStopping){

    openStream();

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->requestStop();
    r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Stopped);

    closeStream();
}


TEST_F(StreamStates, InputStreamStateIsOpenAfterOpening){
    openInputStream();
    StreamState next = StreamState::Unknown;
    Result r = mStream->waitForStateChange(StreamState::Uninitialized, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK) << convertToText(r);
    ASSERT_EQ(next, StreamState::Open) << convertToText(next);
    closeStream();
}

TEST_F(StreamStates, InputStreamStateIsStartedAfterStarting){

    openInputStream();

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Started);

    closeStream();
}

/*
 * TODO: what should the state be when we try to pause an input stream?
 * TEST_F(StreamStates, InputStreamStateIsAfterPausing){

    openInputStream();

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);
    r = mStream->requestPause();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Pausing, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Paused);

    closeStream();
}*/


TEST_F(StreamStates, InputStreamStateIsStoppedAfterStopping){

    openInputStream();

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->requestStop();
    r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Stopped);

    closeStream();
}
