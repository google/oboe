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



class StreamClosedReturnValues : public ::testing::Test {

protected:

    void SetUp(){

    }

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

    void openAndCloseStream(){

        openStream();
        closeStream();
        ASSERT_EQ(mStream->getState(), StreamState::Closed) << "Stream state " << convertToText(mStream->getState());
    }

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;

};

TEST_F(StreamClosedReturnValues, GetChannelCountReturnsLastKnownValue){

    mBuilder.setChannelCount(2);
    openAndCloseStream();
    ASSERT_EQ(mStream->getChannelCount(), 2);
}

TEST_F(StreamClosedReturnValues, GetDirectionReturnsLastKnownValue){

    // Note that when testing on the emulator setting the direction to Input will result in ErrorInternal when
    // opening the stream
    mBuilder.setDirection(Direction::Input);
    openAndCloseStream();
    ASSERT_EQ(mStream->getDirection(), Direction::Input);
}

TEST_F(StreamClosedReturnValues, GetSampleRateReturnsLastKnownValue){

    mBuilder.setSampleRate(8000);
    openAndCloseStream();
    ASSERT_EQ(mStream->getSampleRate(), 8000);
}

TEST_F(StreamClosedReturnValues, GetFramesPerCallbackReturnsLastKnownValue) {

    mBuilder.setFramesPerCallback(192);
    openAndCloseStream();
    ASSERT_EQ(mStream->getFramesPerCallback(), 192);
}

TEST_F(StreamClosedReturnValues, GetFormatReturnsLastKnownValue) {

    mBuilder.setFormat(AudioFormat::I16);
    openAndCloseStream();
    ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
}

TEST_F(StreamClosedReturnValues, GetBufferSizeInFramesReturnsLastKnownValue) {

    openStream();
    int32_t bufferSize = mStream->getBufferSizeInFrames();
    closeStream();
    ASSERT_EQ(mStream->getBufferSizeInFrames(), bufferSize);
}

TEST_F(StreamClosedReturnValues, GetBufferCapacityInFramesReturnsLastKnownValue) {

    openStream();
    int32_t bufferCapacity = mStream->getBufferCapacityInFrames();
    closeStream();
    ASSERT_EQ(mStream->getBufferCapacityInFrames(), bufferCapacity);
}

TEST_F(StreamClosedReturnValues, GetSharingModeReturnsLastKnownValue) {

    openStream();
    SharingMode s = mStream->getSharingMode();
    closeStream();
    ASSERT_EQ(mStream->getSharingMode(), s);
}

TEST_F(StreamClosedReturnValues, GetPerformanceModeReturnsLastKnownValue) {

    openStream();
    PerformanceMode p = mStream->getPerformanceMode();
    closeStream();
    ASSERT_EQ(mStream->getPerformanceMode(), p);
}

TEST_F(StreamClosedReturnValues, GetDeviceIdReturnsLastKnownValue) {

    openStream();
    int32_t d = mStream->getDeviceId();
    closeStream();
    ASSERT_EQ(mStream->getDeviceId(), d);
}

TEST_F(StreamClosedReturnValues, GetCallbackReturnsLastKnownValue) {

    AudioStreamCallback *callback = new MyCallback();
    mBuilder.setCallback(callback);
    openAndCloseStream();

    AudioStreamCallback *callback2 = mStream->getCallback();
    ASSERT_EQ(callback, callback2);
}

TEST_F(StreamClosedReturnValues, GetUsageReturnsLastKnownValue){
    openStream();
    Usage u = mStream->getUsage();
    closeStream();
    ASSERT_EQ(mStream->getUsage(), u);
}

TEST_F(StreamClosedReturnValues, GetContentTypeReturnsLastKnownValue){
    openStream();
    ContentType c = mStream->getContentType();
    closeStream();
    ASSERT_EQ(mStream->getContentType(), c);
}

TEST_F(StreamClosedReturnValues, GetInputPresetReturnsLastKnownValue){
    openStream();
    auto i = mStream->getInputPreset();
    closeStream();
    ASSERT_EQ(mStream->getInputPreset(), i);
}

TEST_F(StreamClosedReturnValues, GetSessionIdReturnsLastKnownValue){
    openStream();
    auto s = mStream->getSessionId();
    closeStream();
    ASSERT_EQ(mStream->getSessionId(), s);
}

TEST_F(StreamClosedReturnValues, StreamStateIsClosed){
    openAndCloseStream();
    ASSERT_EQ(mStream->getState(), StreamState::Closed);
}

TEST_F(StreamClosedReturnValues, GetXRunCountReturnsLastKnownValue){

    openStream();
    if (mStream->isXRunCountSupported()){
        auto i = mStream->getXRunCount();
        ASSERT_EQ(mStream->getXRunCount(), i);
    }
    closeStream();
}

TEST_F(StreamClosedReturnValues, GetFramesPerBurstReturnsLastKnownValue){

    openStream();
    auto f = mStream->getFramesPerBurst();
    closeStream();
    ASSERT_EQ(mStream->getFramesPerBurst(), f);
}

TEST_F(StreamClosedReturnValues, GetBytesPerFrameReturnsLastKnownValue){
    openStream();
    auto f = mStream->getBytesPerFrame();
    closeStream();
    ASSERT_EQ(mStream->getBytesPerFrame(), f);
}

TEST_F(StreamClosedReturnValues, GetBytesPerSampleReturnsLastKnownValue){
    openStream();
    auto f = mStream->getBytesPerSample();
    closeStream();
    ASSERT_EQ(mStream->getBytesPerSample(), f);
}

TEST_F(StreamClosedReturnValues, GetFramesWrittenReturnsLastKnownValue){
    mBuilder.setFormat(AudioFormat::I16);
    mBuilder.setChannelCount(1);
    openStream();
    mStream->start();

    int16_t buffer[4] = { 1, 2, 3, 4 };
    Result r = mStream->write(&buffer, 4, 0);
    if (r != Result::OK){
        FAIL() << "Could not write to audio stream";
    }

    auto f = mStream->getFramesWritten();
    ASSERT_EQ(f, 4);

    closeStream();
    ASSERT_EQ(mStream->getFramesWritten(), f);
}

// TODO: Reading a positive value doesn't work on OpenSL ES in this test - why?
TEST_F(StreamClosedReturnValues, GetFramesReadReturnsLastKnownValue) {

    mBuilder.setDirection(Direction::Input);
    mBuilder.setFormat(AudioFormat::I16);
    mBuilder.setChannelCount(1);

    if (openStream()){
        mStream->start();

/*
        int16_t buffer[192];
        auto r = mStream->read(&buffer, 192, 0);
        ASSERT_EQ(r.value(), 192);
*/

        auto f = mStream->getFramesRead();
//        ASSERT_EQ(f, 192);

        closeStream();
        ASSERT_EQ(mStream->getFramesRead(), f);
    };
}

TEST_F(StreamClosedReturnValues, GetTimestampReturnsErrorClosedIfSupported){

    openStream();

    int64_t framePosition;
    int64_t presentationTime;

    auto r = mStream->getTimestamp(CLOCK_MONOTONIC, &framePosition, &presentationTime);
    bool isTimestampSupported = (r == Result::OK);

    closeStream();

    if (isTimestampSupported){
        ASSERT_EQ(mStream->getTimestamp(CLOCK_MONOTONIC, &framePosition, &presentationTime), Result::ErrorClosed);
    }
}

TEST_F(StreamClosedReturnValues, GetAudioApiReturnsLastKnownValue){
    openStream();
    AudioApi a = mStream->getAudioApi();
    closeStream();
    ASSERT_EQ(mStream->getAudioApi(), a);
}

TEST_F(StreamClosedReturnValues, GetUsesAAudioReturnsLastKnownValue){
    openStream();
    bool a = mStream->usesAAudio();
    closeStream();
    ASSERT_EQ(mStream->usesAAudio(), a);
}

TEST_F(StreamClosedReturnValues, StreamStateControlsReturnClosed){

    openAndCloseStream();
    Result r = mStream->close();
    EXPECT_EQ(r, Result::ErrorClosed) << convertToText(r);
    r = mStream->start();
    EXPECT_EQ(r, Result::ErrorClosed) << convertToText(r);
    EXPECT_EQ(mStream->pause(), Result::ErrorClosed);
    EXPECT_EQ(mStream->flush(), Result::ErrorClosed);
    EXPECT_EQ(mStream->stop(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestStart(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestPause(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestFlush(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestStop(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, WaitForStateChangeReturnsClosed){

    openAndCloseStream();
    StreamState next;
    Result r = mStream->waitForStateChange(StreamState::Open, &next, 0);
    ASSERT_EQ(r, Result::ErrorClosed) << convertToText(r);
}

TEST_F(StreamClosedReturnValues, SetBufferSizeInFramesReturnsClosed){

    openAndCloseStream();
    auto r = mStream->setBufferSizeInFrames(192);
    ASSERT_EQ(r.error(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, CalculateLatencyInMillisReturnsClosedIfSupported){

    openAndCloseStream();

    if (mStream->getAudioApi() == AudioApi::AAudio){
        auto r = mStream->calculateLatencyMillis();
        ASSERT_EQ(r.error(), Result::ErrorClosed);
    }
}

TEST_F(StreamClosedReturnValues, ReadReturnsClosed){

    openAndCloseStream();

    int buffer[8]{0};
    auto r = mStream->read(buffer, 1, 0);
    ASSERT_EQ(r.error(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, WriteReturnsClosed){

    openAndCloseStream();

    int buffer[8]{0};
    auto r = mStream->write(buffer, 1, 0);
    ASSERT_EQ(r.error(), Result::ErrorClosed);
}
