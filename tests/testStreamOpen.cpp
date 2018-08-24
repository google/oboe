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
#include <android/api-level.h>

using namespace oboe;


class StreamOpen : public ::testing::Test {

protected:

    bool openStream(){
        Result r = mBuilder.openStream(&mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    void closeStream(){
        if (mStream != nullptr){
            Result r = mStream->close();
            if (r != Result::OK){
                FAIL() << "Failed to close stream. " << convertToText(r);
            }
        }
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;

};

TEST_F(StreamOpen, ForOpenSLESDefaultSampleRateIsUsed){

    DefaultStreamValues::SampleRate = 44100;
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    openStream();
    ASSERT_EQ(mStream->getSampleRate(), 44100);
    closeStream();
}

TEST_F(StreamOpen, ForOpenSLESDefaultFramesPerBurstIsUsed){

    DefaultStreamValues::FramesPerBurst = 128;
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    openStream();
    ASSERT_EQ(mStream->getFramesPerBurst(), 128);
    closeStream();
}

TEST_F(StreamOpen, ForOpenSLESDefaultChannelCountIsUsed){

    DefaultStreamValues::ChannelCount = 1;
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    openStream();
    ASSERT_EQ(mStream->getChannelCount(), 1);
    closeStream();
}

TEST_F(StreamOpen, OutputForOpenSLESPerformanceModeShouldBeNone){
    // We will not get a LowLatency stream if we request 16000 Hz.
    mBuilder.setSampleRate(16000);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Output);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
	openStream();
    ASSERT_EQ((int)mStream->getPerformanceMode(), (int)PerformanceMode::None);
    closeStream();
}

TEST_F(StreamOpen, InputForOpenSLESPerformanceModeShouldBeNone){
    // We will not get a LowLatency stream if we request 16000 Hz.
    mBuilder.setSampleRate(16000);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Input);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
	openStream();
	ASSERT_EQ((int)mStream->getPerformanceMode(), (int)PerformanceMode::None);
    closeStream();
}
TEST_F(StreamOpen, RecordingFormatUnspecifiedReturnsI16BeforeMarshmallow){

    if (getSdkVersion() < __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Unspecified);
        openStream();
        ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
        closeStream();
    }
}

TEST_F(StreamOpen, RecordingFormatUnspecifiedReturnsFloatOnMarshmallowAndLater){

    if (getSdkVersion() >= __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Unspecified);
        openStream();
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        closeStream();
    }
}

TEST_F(StreamOpen, RecordingFormatFloatReturnsErrorBeforeMarshmallow){

    if (getSdkVersion() < __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Float);
        Result r = mBuilder.openStream(&mStream);
        ASSERT_EQ(r, Result::ErrorInvalidFormat) << convertToText(r);
        closeStream();
    }
}

TEST_F(StreamOpen, RecordingFormatFloatReturnsFloatOnMarshmallowAndLater){

    if (getSdkVersion() >= __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Float);
        openStream();
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        closeStream();
    }
}

TEST_F(StreamOpen, RecordingFormatI16ReturnsI16){

    mBuilder.setDirection(Direction::Input);
    mBuilder.setFormat(AudioFormat::I16);
    openStream();
    ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
    closeStream();
}

TEST_F(StreamOpen, PlaybackFormatUnspecifiedReturnsI16BeforeLollipop){

    if (getSdkVersion() < __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Unspecified);
        openStream();
        ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
        closeStream();
    }
}

TEST_F(StreamOpen, PlaybackFormatUnspecifiedReturnsFloatOnLollipopAndLater){

    if (getSdkVersion() >= __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Unspecified);
        openStream();
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        closeStream();
    }
}

TEST_F(StreamOpen, PlaybackFormatFloatReturnsErrorBeforeLollipop){

    if (getSdkVersion() < __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Float);
        Result r = mBuilder.openStream(&mStream);
        ASSERT_EQ(r, Result::ErrorInvalidFormat);
        closeStream();
    }
}

TEST_F(StreamOpen, PlaybackFormatFloatReturnsFloatOnLollipopAndLater){

    if (getSdkVersion() >= __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Float);
        openStream();
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        closeStream();
    }
}

TEST_F(StreamOpen, PlaybackFormatI16ReturnsI16){

    mBuilder.setDirection(Direction::Output);
    mBuilder.setFormat(AudioFormat::I16);
    openStream();
    ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
    closeStream();
}
