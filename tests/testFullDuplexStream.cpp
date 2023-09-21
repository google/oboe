/*
 * Copyright 2023 The Android Open Source Project
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

class MyFullDuplexStream : public FullDuplexStream {
public:
    DataCallbackResult onBothStreamsReady(
            const void *inputData,
            int numInputFrames,
            void *outputData,
            int numOutputFrames) override {
        lastNumInputFrames = numInputFrames;
        lastNumOutputFrames = numOutputFrames;
        callbackCount++;
        return DataCallbackResult::Continue;
    }

    // This is exposed publicly so that the number of callbacks can be tested.
    std::atomic<int32_t> lastNumInputFrames{0};
    std::atomic<int32_t> lastNumOutputFrames{0};
    std::atomic<int32_t> callbackCount{0};
};

using TestFullDuplexStreamParams = std::tuple<AudioApi, PerformanceMode, AudioApi, PerformanceMode>;

class TestFullDuplexStream : public ::testing::Test,
                         public ::testing::WithParamInterface<TestFullDuplexStreamParams> {

protected:

    void SetUp(){
        mInputBuilder.setDirection(Direction::Input);
        mOutputBuilder.setDirection(Direction::Output);
    }

    void open(AudioApi inputAudioApi, PerformanceMode inputPerfMode,
            AudioApi outputAudioApi, PerformanceMode outputPerfMode) {
        if (mInputBuilder.isAAudioRecommended()) {
            mInputBuilder.setAudioApi(inputAudioApi);
        }
        mInputBuilder.setPerformanceMode(inputPerfMode);
        mInputBuilder.setChannelCount(1);
        mInputBuilder.setFormat(AudioFormat::I16);

        Result r = mInputBuilder.openStream(&mInputStream);
        ASSERT_EQ(r, Result::OK) << "Failed to open input stream " << convertToText(r);

        if (mOutputBuilder.isAAudioRecommended()) {
            mOutputBuilder.setAudioApi(outputAudioApi);
        }
        mOutputBuilder.setPerformanceMode(outputPerfMode);
        mOutputBuilder.setChannelCount(1);
        mOutputBuilder.setFormat(AudioFormat::I16);
        mOutputBuilder.setDataCallback(&mFullDuplexStream);

        r = mOutputBuilder.openStream(&mOutputStream);
        ASSERT_EQ(r, Result::OK) << "Failed to open output stream " << convertToText(r);

        mFullDuplexStream.setInputStream(mInputStream);
        mFullDuplexStream.setOutputStream(mOutputStream);
    }

    void start() {
        Result r = mFullDuplexStream.start();
        ASSERT_EQ(r, Result::OK) << "Failed to start streams " << convertToText(r);
    }

    void stop() {
        Result r = mFullDuplexStream.stop();
        ASSERT_EQ(r, Result::OK) << "Failed to stop streams " << convertToText(r);
    }

    void close() {
        Result r = mOutputStream->close();
        ASSERT_EQ(r, Result::OK) << "Failed to close output stream " << convertToText(r);
        mFullDuplexStream.setOutputStream(nullptr);
        r = mInputStream->close();
        ASSERT_EQ(r, Result::OK) << "Failed to close input stream " << convertToText(r);
        mFullDuplexStream.setInputStream(nullptr);
    }

    void sleepUntilFirstCallbackWithBothInputAndOutputFrames() {
        int numAttempts = 0;
        while (mFullDuplexStream.callbackCount == 0 || mFullDuplexStream.lastNumInputFrames == 0 ||
                mFullDuplexStream.lastNumOutputFrames == 0) {
            numAttempts++;
            usleep(kTimeToSleepMicros);
            ASSERT_LE(numAttempts, kMaxSleepAttempts);
        }
    }

    void checkFrameCount() {
        ASSERT_GT(mFullDuplexStream.callbackCount, 0);
        ASSERT_GT(mFullDuplexStream.lastNumInputFrames, 0);
        ASSERT_GT(mFullDuplexStream.lastNumOutputFrames, 0);
    }

    AudioStreamBuilder mInputBuilder;
    AudioStreamBuilder mOutputBuilder;
    AudioStream *mInputStream = nullptr;
    AudioStream *mOutputStream = nullptr;
    MyFullDuplexStream mFullDuplexStream;
    static constexpr int kTimeToSleepMicros = 50 * 1000; // 50 ms
    static constexpr int kMaxSleepAttempts = 100;
    static constexpr int kMinInputFrames = 1;
};

TEST_P(TestFullDuplexStream, VerifyFullDuplexStream) {
    const AudioApi inputAudioApi = std::get<0>(GetParam());
    const PerformanceMode inputPerformanceMode = std::get<1>(GetParam());
    const AudioApi outputAudioApi = std::get<2>(GetParam());
    const PerformanceMode outputPerformanceMode = std::get<3>(GetParam());

    open(inputAudioApi, inputPerformanceMode, outputAudioApi, outputPerformanceMode);
    start();
    sleepUntilFirstCallbackWithBothInputAndOutputFrames();
    stop();
    checkFrameCount();
    close();
}

INSTANTIATE_TEST_SUITE_P(
        TestFullDuplexStreamTest,
        TestFullDuplexStream,
        ::testing::Values(
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving})
        )
);
