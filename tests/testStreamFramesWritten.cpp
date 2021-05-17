/*
 * Copyright 2021 The Android Open Source Project
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

#include <tuple>

using namespace oboe;

class MyCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        return DataCallbackResult::Continue;
    }
};

using StreamFramesWrittenParams = std::tuple<Direction, int32_t>;

class StreamFramesWritten : public ::testing::Test,
                            public ::testing::WithParamInterface<StreamFramesWrittenParams> {

protected:
    void TearDown() override;

    static constexpr int PROCESS_TIME_SECOND = 5;

    AudioStreamBuilder mBuilder;
    AudioStream *mStream = nullptr;
};

void StreamFramesWritten::TearDown() {
    if (mStream != nullptr) {
        mStream->close();
        mStream = nullptr;
    }
}

TEST_P(StreamFramesWritten, VerifyFramesWritten) {
    const Direction direction = std::get<0>(GetParam());
    const int32_t sampleRate = std::get<1>(GetParam());

    AudioStreamDataCallback *callback = new MyCallback();
    mBuilder.setDirection(direction)
            ->setFormat(AudioFormat::Float)
            ->setSampleRate(sampleRate)
            ->setSampleRateConversionQuality(SampleRateConversionQuality::Medium)
            ->setPerformanceMode(PerformanceMode::LowLatency)
            ->setSharingMode(SharingMode::Exclusive)
            ->setDataCallback(callback);
    mStream = nullptr;
    Result r = mBuilder.openStream(&mStream);
    ASSERT_EQ(r, Result::OK) << "Failed to open stream." << convertToText(r);

    r = mStream->start();
    ASSERT_EQ(r, Result::OK) << "Failed to start stream." << convertToText(r);
    sleep(PROCESS_TIME_SECOND);

    // The frames written should be close to sampleRate * PROCESS_TIME_SECONDS
    const int64_t framesWritten = mStream->getFramesWritten();
    EXPECT_NEAR(framesWritten, sampleRate * PROCESS_TIME_SECOND, sampleRate / 2);
}

INSTANTIATE_TEST_CASE_P(
        StreamsFramesWrittenTest,
        StreamFramesWritten,
        ::testing::Values(
                StreamFramesWrittenParams({Direction::Output, 8000}),
                StreamFramesWrittenParams({Direction::Output, 16000}),
                StreamFramesWrittenParams({Direction::Output, 44100}),
                StreamFramesWrittenParams({Direction::Input, 8000}),
                StreamFramesWrittenParams({Direction::Input, 16000}),
                StreamFramesWrittenParams({Direction::Input, 44100})
                )
        );
