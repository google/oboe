#include <oboe/Oboe.h>/*
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


class StreamOpen : public ::testing::Test {

protected:

    bool openStream(AudioApi api){
        mBuilder.setAudioApi(api);
        Result r = mBuilder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        EXPECT_EQ(mStream->getAudioApi(), AudioApi::OpenSLES) << "Stream is not using OpenSLES";

        return (r == Result::OK);
    }

    void closeStream(){
        Result r = mStream->close();
        if (r != Result::OK){
            FAIL() << "Failed to close stream. " << convertToText(r);
        }
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;

};

TEST_F(StreamOpen, ForOpenSLESDefaultSampleRateIsUsed){

    DefaultStreamValues::SampleRate = 44100;

    openStream(AudioApi::OpenSLES);
    ASSERT_EQ(mStream->getSampleRate(), 44100);
    closeStream();
}

TEST_F(StreamOpen, ForOpenSLESDefaultFramesPerBurstIsUsed){

    DefaultStreamValues::FramesPerBurst = 128;
    openStream(AudioApi::OpenSLES);
    ASSERT_EQ(mStream->getFramesPerBurst(), 128);
    closeStream();
}

TEST_F(StreamOpen, ForOpenSLESDefaultChannelCountIsUsed){

    DefaultStreamValues::ChannelCount = 1;
    openStream(AudioApi::OpenSLES);
    ASSERT_EQ(mStream->getChannelCount(), 1);
    closeStream();
}

TEST_F(StreamOpen, OutputForOpenSLESPerformanceModeShouldBeNone){
    // We will not get a LowLatency stream if we request 16000 Hz.
    mBuilder.setSampleRate(16000);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Output);
    openStream(AudioApi::OpenSLES);
    ASSERT_EQ((int)mStream->getPerformanceMode(), (int)PerformanceMode::None);
    closeStream();
}

TEST_F(StreamOpen, InputForOpenSLESPerformanceModeShouldBeNone){
    // We will not get a LowLatency stream if we request 16000 Hz.
    mBuilder.setSampleRate(16000);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Input);
    openStream(AudioApi::OpenSLES);
    ASSERT_EQ((int)mStream->getPerformanceMode(), (int)PerformanceMode::None);
    closeStream();
}
