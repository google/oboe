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


class MyCallback : public AudioStreamCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        return DataCallbackResult::Continue;
    }
};

class XRunBehaviour : public ::testing::Test {

protected:

    void SetUp(){

    }


    void openStream(){
        Result r = mBuilder.openStream(&mStream);
        if (r != Result::OK){
            FAIL() << "Failed to open stream. " << convertToText(r);
        }
    }

    void closeStream(){
        if (mStream != nullptr){
            Result r = mStream->close();
            if (r != Result::OK){
                FAIL() << "Failed to close stream. " << convertToText(r);
            }
        }
    }

    void openAndCloseStream(){

        openStream();
        closeStream();
        ASSERT_EQ(mStream->getState(), StreamState::Closed);
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;

};

// TODO figure out this behaviour - On OpenSLES xRuns are supported withing AudioStreamBuffered, however, these aren't
// the same as the actual stream underruns
TEST_F(XRunBehaviour, SupportedWhenStreamIsUsingAAudio){

    openStream();
    if (mStream->getAudioApi() == AudioApi::AAudio){
        ASSERT_TRUE(mStream->isXRunCountSupported());
    }
    closeStream();
}

TEST_F(XRunBehaviour, NotSupportedOnOpenSLESWhenStreamIsUsingCallback){

    MyCallback callback;
    mBuilder.setCallback(&callback);
    openStream();
    if (mStream->getAudioApi() == AudioApi::OpenSLES){
        ASSERT_FALSE(mStream->isXRunCountSupported());
    }
    closeStream();
}

TEST_F(XRunBehaviour, SupportedOnOpenSLESWhenStreamIsUsingBlockingIO){

    openStream();
    if (mStream->getAudioApi() == AudioApi::OpenSLES){
        ASSERT_TRUE(mStream->isXRunCountSupported());
    }
    closeStream();
}