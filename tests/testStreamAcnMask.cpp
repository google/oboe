/*
 * Copyright 2026 The Android Open Source Project
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

#include <atomic>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <oboe/Oboe.h>

#include "common/QuirksManager.h"

using namespace oboe;

namespace {

constexpr int kAllOrders = 15;

// All of the valid ambisonic channel masks, in ACN order.
const std::vector<ChannelAcnMask> kAllAcnMasks = {
        ChannelAcnMask::Order0, ChannelAcnMask::Order1, ChannelAcnMask::Order2,
        ChannelAcnMask::Order3, ChannelAcnMask::Order4, ChannelAcnMask::Order5,
        ChannelAcnMask::Order6, ChannelAcnMask::Order7, ChannelAcnMask::Order8,
        ChannelAcnMask::Order9, ChannelAcnMask::Order10, ChannelAcnMask::Order11,
        ChannelAcnMask::Order12, ChannelAcnMask::Order13, ChannelAcnMask::Order14,
        ChannelAcnMask::Order0Hrz, ChannelAcnMask::Order1Hrz, ChannelAcnMask::Order2Hrz,
        ChannelAcnMask::Order3Hrz, ChannelAcnMask::Order4Hrz, ChannelAcnMask::Order5Hrz,
        ChannelAcnMask::Order6Hrz, ChannelAcnMask::Order7Hrz, ChannelAcnMask::Order8Hrz,
        ChannelAcnMask::Order9Hrz, ChannelAcnMask::Order10Hrz, ChannelAcnMask::Order11Hrz,
        ChannelAcnMask::Order12Hrz, ChannelAcnMask::Order13Hrz, ChannelAcnMask::Order14Hrz,
};

// Values that are not valid ambisonic channel masks.
const std::vector<uint32_t> kInvalidAcnMasks = {
        2, 3, 5, 8, 10, 226, 255,                // counts that are not (N + 1)^2
        1u << 9,                                  // Horizontal flag only
        (1u << 9) | 2, (1u << 9) | 4, (1u << 9) | 31, // even or too large horizontal counts
        (1u << 8) | 4,                            // unknown bit 8
        (1u << 10) | 4,                           // unknown bit 10
        0x80000000u | 4,                          // unknown high bit
};

int expectedChannelCount(ChannelAcnMask mask) {
    const uint32_t value = static_cast<uint32_t>(mask);
    return static_cast<int>(value & 0xFF);
}

bool isHorizontal(ChannelAcnMask mask) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(ChannelAcnMask::Horizontal)) != 0;
}

std::string acnMaskName(ChannelAcnMask mask) {
    return convertToText(mask);
}

class AcnCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *stream, void *audioData,
                                    int32_t numFrames) override {
        if (stream->getChannelCount() != expectedChannelCount) {
            badChannelCount = true;
        }
        if (stream->getDirection() == Direction::Output) {
            memset(audioData, 0, static_cast<size_t>(numFrames) * stream->getBytesPerFrame());
        }
        callbackCount++;
        framesProcessed += numFrames;
        return DataCallbackResult::Continue;
    }

    int32_t expectedChannelCount = 0;
    std::atomic<bool> badChannelCount{false};
    std::atomic<int32_t> callbackCount{0};
    std::atomic<int64_t> framesProcessed{0};
};

class AcnErrorCallback : public AudioStreamErrorCallback {
public:
    void onErrorAfterClose(AudioStream * /* stream */, Result error) override {
        lastError = error;
        errorCount++;
    }

    std::atomic<Result> lastError{Result::OK};
    std::atomic<int32_t> errorCount{0};
};

} // namespace

// ---------------------------------------------------------------------------------------------
// Tests that do not open a stream.

TEST(StreamAcnMaskDefinitions, ValuesMatchAAudio) {
    // These values are defined by AAudio_ChannelAcnMask in AAudio.h (API 38).
    for (int order = 0; order < kAllOrders; order++) {
        EXPECT_EQ(static_cast<uint32_t>(kAllAcnMasks[order]),
                  static_cast<uint32_t>((order + 1) * (order + 1))) << "order " << order;
        EXPECT_EQ(static_cast<uint32_t>(kAllAcnMasks[kAllOrders + order]),
                  (1u << 9) | static_cast<uint32_t>(2 * order + 1)) << "hrz order " << order;
    }
    EXPECT_EQ(0u, static_cast<uint32_t>(ChannelAcnMask::Unspecified));
    EXPECT_EQ(1u << 9, static_cast<uint32_t>(ChannelAcnMask::Horizontal));
    EXPECT_EQ(0xFFu, static_cast<uint32_t>(ChannelAcnMask::CountMask));
}

TEST(StreamAcnMaskDefinitions, ChannelCountFromMask) {
    for (int order = 0; order < kAllOrders; order++) {
        EXPECT_EQ((order + 1) * (order + 1), getChannelCountFromChannelAcnMask(kAllAcnMasks[order]));
        EXPECT_EQ(2 * order + 1,
                  getChannelCountFromChannelAcnMask(kAllAcnMasks[kAllOrders + order]));
    }
    EXPECT_EQ(0, getChannelCountFromChannelAcnMask(ChannelAcnMask::Unspecified));
}

TEST(StreamAcnMaskDefinitions, IsValidChannelAcnMask) {
    for (ChannelAcnMask mask : kAllAcnMasks) {
        EXPECT_TRUE(isValidChannelAcnMask(mask)) << acnMaskName(mask);
    }
    EXPECT_FALSE(isValidChannelAcnMask(ChannelAcnMask::Unspecified));
    EXPECT_FALSE(isValidChannelAcnMask(ChannelAcnMask::Horizontal));
    EXPECT_FALSE(isValidChannelAcnMask(ChannelAcnMask::CountMask));
    for (uint32_t value : kInvalidAcnMasks) {
        EXPECT_FALSE(isValidChannelAcnMask(static_cast<ChannelAcnMask>(value)))
                << std::hex << value;
    }
}

TEST(StreamAcnMaskDefinitions, ConvertToText) {
    EXPECT_STREQ("Unspecified", convertToText(ChannelAcnMask::Unspecified));
    EXPECT_STREQ("Order0", convertToText(ChannelAcnMask::Order0));
    EXPECT_STREQ("Order14", convertToText(ChannelAcnMask::Order14));
    EXPECT_STREQ("Order1Hrz", convertToText(ChannelAcnMask::Order1Hrz));
    EXPECT_STREQ("Order14Hrz", convertToText(ChannelAcnMask::Order14Hrz));
    for (ChannelAcnMask mask : kAllAcnMasks) {
        const std::string text = convertToText(mask);
        EXPECT_EQ(std::string::npos, text.find("Unrecognized")) << text;
        EXPECT_EQ(isHorizontal(mask), text.find("Hrz") != std::string::npos) << text;
    }
    EXPECT_STREQ("Unrecognized channel ACN mask",
                 convertToText(static_cast<ChannelAcnMask>(3)));
}

TEST(StreamAcnMaskBuilder, DefaultIsUnspecified) {
    AudioStreamBuilder builder;
    EXPECT_EQ(ChannelAcnMask::Unspecified, builder.getChannelAcnMask());
    EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
    EXPECT_EQ(kUnspecified, builder.getChannelCount());
}

TEST(StreamAcnMaskBuilder, SetDerivesChannelCount) {
    for (ChannelAcnMask mask : kAllAcnMasks) {
        AudioStreamBuilder builder;
        builder.setChannelAcnMask(mask);
        EXPECT_EQ(mask, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
        EXPECT_EQ(expectedChannelCount(mask), builder.getChannelCount()) << acnMaskName(mask);
    }
}

TEST(StreamAcnMaskBuilder, LastCallWins) {
    {   // count then ACN
        AudioStreamBuilder builder;
        builder.setChannelCount(2)->setChannelAcnMask(ChannelAcnMask::Order1);
        EXPECT_EQ(ChannelAcnMask::Order1, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
        EXPECT_EQ(4, builder.getChannelCount());
    }
    {   // ACN then count
        AudioStreamBuilder builder;
        builder.setChannelAcnMask(ChannelAcnMask::Order1)->setChannelCount(2);
        EXPECT_EQ(ChannelAcnMask::Unspecified, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
        EXPECT_EQ(2, builder.getChannelCount());
    }
    {   // mask then ACN
        AudioStreamBuilder builder;
        builder.setChannelMask(ChannelMask::CM5Point1)->setChannelAcnMask(ChannelAcnMask::Order2Hrz);
        EXPECT_EQ(ChannelAcnMask::Order2Hrz, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
        EXPECT_EQ(5, builder.getChannelCount());
    }
    {   // ACN then mask
        AudioStreamBuilder builder;
        builder.setChannelAcnMask(ChannelAcnMask::Order2Hrz)->setChannelMask(ChannelMask::Stereo);
        EXPECT_EQ(ChannelAcnMask::Unspecified, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Stereo, builder.getChannelMask());
        EXPECT_EQ(2, builder.getChannelCount());
    }
    {   // ACN then ACN
        AudioStreamBuilder builder;
        builder.setChannelAcnMask(ChannelAcnMask::Order3)->setChannelAcnMask(ChannelAcnMask::Order0);
        EXPECT_EQ(ChannelAcnMask::Order0, builder.getChannelAcnMask());
        EXPECT_EQ(1, builder.getChannelCount());
    }
    {   // count, mask, ACN
        AudioStreamBuilder builder;
        builder.setChannelCount(1)->setChannelMask(ChannelMask::Stereo)
                ->setChannelAcnMask(ChannelAcnMask::Order14);
        EXPECT_EQ(ChannelAcnMask::Order14, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
        EXPECT_EQ(225, builder.getChannelCount());
    }
    {   // Unspecified ACN clears the ACN mask and the channel count.
        AudioStreamBuilder builder;
        builder.setChannelAcnMask(ChannelAcnMask::Order1)
                ->setChannelAcnMask(ChannelAcnMask::Unspecified);
        EXPECT_EQ(ChannelAcnMask::Unspecified, builder.getChannelAcnMask());
        EXPECT_EQ(ChannelMask::Unspecified, builder.getChannelMask());
        EXPECT_EQ(kUnspecified, builder.getChannelCount());
    }
    {   // Unspecified ACN after a channel count also clears the count, like AAudio.
        AudioStreamBuilder builder;
        builder.setChannelCount(2)->setChannelAcnMask(ChannelAcnMask::Unspecified);
        EXPECT_EQ(ChannelAcnMask::Unspecified, builder.getChannelAcnMask());
        EXPECT_EQ(kUnspecified, builder.getChannelCount());
    }
}

TEST(StreamAcnMaskBuilder, CopiedToChildBuilder) {
    AudioStreamBuilder builder;
    builder.setChannelAcnMask(ChannelAcnMask::Order3Hrz);
    AudioStreamBuilder copy(builder);
    EXPECT_EQ(ChannelAcnMask::Order3Hrz, copy.getChannelAcnMask());
    EXPECT_EQ(7, copy.getChannelCount());
}

// QuirksManager must never change the channel count or ACN mask of an ambisonic stream.
TEST(StreamAcnMaskBuilder, QuirksDoNotConvertChannels) {
    for (Direction direction : {Direction::Input, Direction::Output}) {
        for (ChannelAcnMask mask : {ChannelAcnMask::Order0, ChannelAcnMask::Order0Hrz,
                                    ChannelAcnMask::Order1, ChannelAcnMask::Order1Hrz}) {
            AudioStreamBuilder builder;
            builder.setDirection(direction)
                    ->setChannelAcnMask(mask)
                    ->setChannelConversionAllowed(true)
                    ->setFormatConversionAllowed(true)
                    ->setPerformanceMode(PerformanceMode::LowLatency)
                    ->setSampleRate(44100);
            AudioStreamBuilder childBuilder(builder);
            QuirksManager::getInstance().isConversionNeeded(builder, childBuilder);
            EXPECT_EQ(mask, childBuilder.getChannelAcnMask()) << acnMaskName(mask);
            EXPECT_EQ(expectedChannelCount(mask), childBuilder.getChannelCount());
            EXPECT_EQ(ChannelMask::Unspecified, childBuilder.getChannelMask());
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Tests that open streams.

class StreamAcnMask : public ::testing::Test {
protected:
    void TearDown() override {
        if (mStream) {
            mStream->close();
            mStream.reset();
            usleep(100 * 1000); // give the stream time to settle
        }
    }

    Result openStream() {
        EXPECT_EQ(nullptr, mStream);
        Result result = mBuilder.openStream(mStream);
        if (result != Result::OK) {
            EXPECT_EQ(nullptr, mStream);
        }
        return result;
    }

    void closeStream() {
        if (mStream) {
            EXPECT_EQ(Result::OK, mStream->close());
            mStream.reset();
            usleep(100 * 1000);
        }
    }

    // Check the stream properties that must hold for any successfully opened ACN stream.
    void checkOpenedAcnStream(ChannelAcnMask mask) {
        ASSERT_NE(nullptr, mStream);
        EXPECT_EQ(AudioApi::AAudio, mStream->getAudioApi());
        EXPECT_EQ(mask, mStream->getChannelAcnMask()) << acnMaskName(mask);
        EXPECT_EQ(ChannelMask::Unspecified, mStream->getChannelMask()) << acnMaskName(mask);
        EXPECT_EQ(expectedChannelCount(mask), mStream->getChannelCount()) << acnMaskName(mask);
        EXPECT_EQ(expectedChannelCount(mask) * mStream->getBytesPerSample(),
                  mStream->getBytesPerFrame());
        // AAudio does not use MMAP for ambisonic streams.
        EXPECT_FALSE(OboeExtensions::isMMapUsed(mStream.get())) << acnMaskName(mask);
    }

    static bool isSupported() {
        return AudioStreamBuilder::isAAudioRecommended()
                && OboeExtensions::isChannelAcnMaskSupported();
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
};

TEST_F(StreamAcnMask, UnsetIsUnspecifiedAfterOpen) {
    for (Direction direction : {Direction::Output, Direction::Input}) {
        mBuilder.setDirection(direction)->setChannelCount(2);
        ASSERT_EQ(Result::OK, openStream());
        EXPECT_EQ(ChannelAcnMask::Unspecified, mStream->getChannelAcnMask());
        closeStream();
        mBuilder.setChannelMask(ChannelMask::Stereo);
        ASSERT_EQ(Result::OK, openStream());
        EXPECT_EQ(ChannelAcnMask::Unspecified, mStream->getChannelAcnMask());
        closeStream();
        // ACN set and then overridden by a channel count.
        mBuilder.setChannelAcnMask(ChannelAcnMask::Order1)->setChannelCount(1);
        ASSERT_EQ(Result::OK, openStream());
        EXPECT_EQ(ChannelAcnMask::Unspecified, mStream->getChannelAcnMask());
        EXPECT_EQ(1, mStream->getChannelCount());
        closeStream();
    }
}

TEST_F(StreamAcnMask, InvalidMaskFailsWithIllegalArgument) {
    for (Direction direction : {Direction::Output, Direction::Input}) {
        for (uint32_t value : kInvalidAcnMasks) {
            mBuilder.setDirection(direction)->setChannelAcnMask(static_cast<ChannelAcnMask>(value));
            EXPECT_EQ(Result::ErrorIllegalArgument, openStream()) << std::hex << value;
            closeStream();
        }
    }
}

TEST_F(StreamAcnMask, OpenSLESFailsCleanly) {
    for (Direction direction : {Direction::Output, Direction::Input}) {
        mBuilder.setDirection(direction)
                ->setAudioApi(AudioApi::OpenSLES)
                ->setChannelAcnMask(ChannelAcnMask::Order1);
        EXPECT_EQ(Result::ErrorUnimplemented, openStream());
        // Also with a format, which is required by OpenSL ES.
        mBuilder.setFormat(AudioFormat::Float);
        EXPECT_EQ(Result::ErrorUnimplemented, openStream());
    }
}

TEST_F(StreamAcnMask, UnsupportedPlatformFailsCleanly) {
    if (isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are supported on this device";
    }
    for (Direction direction : {Direction::Output, Direction::Input}) {
        for (ChannelAcnMask mask : kAllAcnMasks) {
            mBuilder.setDirection(direction)->setChannelAcnMask(mask);
            EXPECT_EQ(Result::ErrorUnimplemented, openStream()) << acnMaskName(mask);
        }
    }
}

TEST_F(StreamAcnMask, SupportedMatchesSdkVersion) {
    if (getSdkVersion() >= 38) {
        EXPECT_TRUE(OboeExtensions::isChannelAcnMaskSupported());
    } else if (getSdkVersion() < 37) {
        EXPECT_FALSE(OboeExtensions::isChannelAcnMaskSupported());
    }
}

// Parameterized over direction x mask.
using AcnOpenParams = std::tuple<Direction, ChannelAcnMask>;

class StreamAcnMaskOpen : public StreamAcnMask,
                          public ::testing::WithParamInterface<AcnOpenParams> {
public:
    static std::string getTestName(const ::testing::TestParamInfo<AcnOpenParams> &info) {
        return std::string(std::get<0>(info.param) == Direction::Output ? "Output_" : "Input_")
                + convertToText(std::get<1>(info.param));
    }
};

TEST_P(StreamAcnMaskOpen, OpenAndReadBack) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    const Direction direction = std::get<0>(GetParam());
    const ChannelAcnMask mask = std::get<1>(GetParam());
    mBuilder.setDirection(direction)->setChannelAcnMask(mask);
    Result result = openStream();
    if (result != Result::OK) {
        // AAudio may fail to open masks that the device cannot support.
        // It must never silently open a positional stream instead.
        GTEST_SKIP() << "open failed with " << convertToText(result);
    }
    checkOpenedAcnStream(mask);
}

TEST_P(StreamAcnMaskOpen, OpenAllPerformanceAndSharingModes) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    const Direction direction = std::get<0>(GetParam());
    const ChannelAcnMask mask = std::get<1>(GetParam());
    for (PerformanceMode perf : {PerformanceMode::None, PerformanceMode::LowLatency,
                                 PerformanceMode::PowerSaving}) {
        for (SharingMode sharing : {SharingMode::Shared, SharingMode::Exclusive}) {
            mBuilder.setDirection(direction)
                    ->setChannelAcnMask(mask)
                    ->setPerformanceMode(perf)
                    ->setSharingMode(sharing);
            Result result = openStream();
            if (result == Result::OK) {
                checkOpenedAcnStream(mask);
                // Without MMAP there is no exclusive stream.
                EXPECT_EQ(SharingMode::Shared, mStream->getSharingMode());
            }
            closeStream();
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
        AllMasks, StreamAcnMaskOpen,
        ::testing::Combine(::testing::Values(Direction::Output, Direction::Input),
                           ::testing::ValuesIn(kAllAcnMasks)),
        &StreamAcnMaskOpen::getTestName);

TEST_F(StreamAcnMask, ChannelMaskThenAcnMaskOpensAcn) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    mBuilder.setDirection(Direction::Input)
            ->setChannelMask(ChannelMask::Stereo)
            ->setChannelAcnMask(ChannelAcnMask::Order1);
    ASSERT_EQ(Result::OK, openStream());
    checkOpenedAcnStream(ChannelAcnMask::Order1);
}

TEST_F(StreamAcnMask, AcnMaskThenChannelMaskOpensPositional) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    mBuilder.setDirection(Direction::Output)
            ->setChannelAcnMask(ChannelAcnMask::Order1)
            ->setChannelMask(ChannelMask::Stereo);
    ASSERT_EQ(Result::OK, openStream());
    EXPECT_EQ(ChannelAcnMask::Unspecified, mStream->getChannelAcnMask());
    EXPECT_EQ(ChannelMask::Stereo, mStream->getChannelMask());
    EXPECT_EQ(2, mStream->getChannelCount());
}

// MMAP is never used for ACN even if requested. Oboe must report the actual values.
TEST_F(StreamAcnMask, MMapIsNotUsed) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    const bool wasMMapEnabled = OboeExtensions::isMMapEnabled();
    OboeExtensions::setMMapEnabled(true);
    for (Direction direction : {Direction::Output, Direction::Input}) {
        mBuilder.setDirection(direction)
                ->setChannelAcnMask(ChannelAcnMask::Order1)
                ->setPerformanceMode(PerformanceMode::LowLatency)
                ->setSharingMode(SharingMode::Exclusive);
        Result result = openStream();
        if (result == Result::OK) {
            checkOpenedAcnStream(ChannelAcnMask::Order1);
            EXPECT_EQ(SharingMode::Shared, mStream->getSharingMode());
        }
        closeStream();
    }
    OboeExtensions::setMMapEnabled(wasMMapEnabled);
}

// Parameterized over direction x format.
using AcnFormatParams = std::tuple<Direction, AudioFormat>;

class StreamAcnMaskFormat : public StreamAcnMask,
                            public ::testing::WithParamInterface<AcnFormatParams> {
public:
    static std::string getTestName(const ::testing::TestParamInfo<AcnFormatParams> &info) {
        return std::string(std::get<0>(info.param) == Direction::Output ? "Output_" : "Input_")
                + convertToText(std::get<1>(info.param));
    }
};

TEST_P(StreamAcnMaskFormat, OpenWithFormat) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    const Direction direction = std::get<0>(GetParam());
    const AudioFormat format = std::get<1>(GetParam());
    for (ChannelAcnMask mask : {ChannelAcnMask::Order1, ChannelAcnMask::Order3Hrz}) {
        mBuilder.setDirection(direction)->setChannelAcnMask(mask)->setFormat(format);
        Result result = openStream();
        if (result == Result::OK) {
            checkOpenedAcnStream(mask);
            EXPECT_EQ(format, mStream->getFormat());
        }
        closeStream();
    }
}

INSTANTIATE_TEST_SUITE_P(
        AllFormats, StreamAcnMaskFormat,
        ::testing::Combine(::testing::Values(Direction::Output, Direction::Input),
                           ::testing::Values(AudioFormat::I16, AudioFormat::Float,
                                             AudioFormat::I24, AudioFormat::I32)),
        &StreamAcnMaskFormat::getTestName);

// Sample rate conversion by Oboe goes through a FilterAudioStream. It must keep the ACN mask.
TEST_F(StreamAcnMask, SampleRateConversionKeepsAcnMask) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    for (Direction direction : {Direction::Output, Direction::Input}) {
        for (ChannelAcnMask mask : {ChannelAcnMask::Order0, ChannelAcnMask::Order1,
                                    ChannelAcnMask::Order2Hrz, ChannelAcnMask::Order3}) {
            mBuilder.setDirection(direction)
                    ->setChannelAcnMask(mask)
                    ->setPerformanceMode(PerformanceMode::LowLatency)
                    ->setSampleRate(44100)
                    ->setSampleRateConversionQuality(SampleRateConversionQuality::Medium)
                    ->setFormat(AudioFormat::Float)
                    ->setFormatConversionAllowed(true)
                    ->setChannelConversionAllowed(true);
            Result result = openStream();
            if (result == Result::OK) {
                checkOpenedAcnStream(mask);
                EXPECT_EQ(44100, mStream->getSampleRate());
                EXPECT_EQ(AudioFormat::Float, mStream->getFormat());
            }
            closeStream();
        }
    }
}

// The masks that are used for the data tests.
const std::vector<ChannelAcnMask> kDataTestMasks = {
        ChannelAcnMask::Order0, ChannelAcnMask::Order1, ChannelAcnMask::Order3,
        ChannelAcnMask::Order7, ChannelAcnMask::Order14, ChannelAcnMask::Order1Hrz,
        ChannelAcnMask::Order3Hrz, ChannelAcnMask::Order14Hrz,
};

// Parameterized over direction x callback x sample rate conversion.
using AcnDataParams = std::tuple<Direction, bool /* callback */, bool /* SRC */>;

class StreamAcnMaskData : public StreamAcnMask,
                          public ::testing::WithParamInterface<AcnDataParams> {
public:
    static std::string getTestName(const ::testing::TestParamInfo<AcnDataParams> &info) {
        return std::string(std::get<0>(info.param) == Direction::Output ? "Output" : "Input")
                + (std::get<1>(info.param) ? "_Callback" : "_Blocking")
                + (std::get<2>(info.param) ? "_SRC" : "");
    }

protected:
    // Returns false if the output could not be rendered.
    bool runData(ChannelAcnMask mask, Direction direction, bool useCallback, bool useSrc);
};

bool StreamAcnMaskData::runData(ChannelAcnMask mask, Direction direction, bool useCallback,
                                bool useSrc) {
    AcnCallback callback;
    callback.expectedChannelCount = expectedChannelCount(mask);
    std::shared_ptr<AcnErrorCallback> errorCallback = std::make_shared<AcnErrorCallback>();
    mBuilder.setDirection(direction)
            ->setChannelAcnMask(mask)
            ->setFormat(AudioFormat::Float)
            ->setErrorCallback(errorCallback);
    if (useCallback) {
        mBuilder.setDataCallback(&callback);
    }
    if (useSrc) {
        mBuilder.setPerformanceMode(PerformanceMode::LowLatency)
                ->setSampleRate(44100)
                ->setSampleRateConversionQuality(SampleRateConversionQuality::Medium);
    }
    Result result = openStream();
    if (result != Result::OK) {
        ADD_FAILURE() << "open failed for " << acnMaskName(mask) << ": " << convertToText(result);
        return true;
    }
    checkOpenedAcnStream(mask);
    EXPECT_EQ(Result::OK, mStream->requestStart());

    bool rendered = true;
    const int32_t channelCount = mStream->getChannelCount();
    if (useCallback) {
        // Wait for data. Output streams may get a few callbacks to prime the buffer.
        const int32_t primeCallbacks = (direction == Direction::Output) ? 8 : 0;
        for (int i = 0; i < 40 && callback.callbackCount <= primeCallbacks; i++) {
            usleep(25 * 1000);
        }
        EXPECT_FALSE(callback.badChannelCount);
        if (direction == Direction::Output && callback.callbackCount <= primeCallbacks) {
            rendered = false;
        } else {
            EXPECT_GT(callback.callbackCount, primeCallbacks) << acnMaskName(mask);
            EXPECT_GT(callback.framesProcessed, 0) << acnMaskName(mask);
        }
    } else {
        const int32_t framesPerTransfer = std::max(mStream->getFramesPerBurst(), 64);
        std::vector<float> buffer(static_cast<size_t>(framesPerTransfer) * channelCount, 0.0f);
        constexpr int64_t kTimeoutNanos = 200 * kNanosPerMillisecond;
        int64_t framesTransferred = 0;
        for (int i = 0; i < 20; i++) {
            ResultWithValue<int32_t> transfer = (direction == Direction::Output)
                    ? mStream->write(buffer.data(), framesPerTransfer, kTimeoutNanos)
                    : mStream->read(buffer.data(), framesPerTransfer, kTimeoutNanos);
            if (!transfer) {
                if (direction == Direction::Output
                        && transfer.error() == Result::ErrorDisconnected) {
                    rendered = false;
                } else {
                    ADD_FAILURE() << acnMaskName(mask) << " transfer failed: "
                                  << convertToText(transfer.error());
                }
                break;
            }
            framesTransferred += transfer.value();
        }
        if (rendered) {
            EXPECT_GT(framesTransferred, 0) << acnMaskName(mask);
            if (direction == Direction::Input) {
                EXPECT_GT(mStream->getFramesRead(), 0);
            } else {
                EXPECT_GT(mStream->getFramesWritten(), 0);
            }
        }
    }
    mStream->requestStop();
    closeStream();
    return rendered;
}

TEST_P(StreamAcnMaskData, StartTransferStop) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    const Direction direction = std::get<0>(GetParam());
    const bool useCallback = std::get<1>(GetParam());
    const bool useSrc = std::get<2>(GetParam());
    std::string unrendered;
    for (ChannelAcnMask mask : kDataTestMasks) {
        mBuilder = AudioStreamBuilder();
        if (!runData(mask, direction, useCallback, useSrc)) {
            unrendered += std::string(" ") + acnMaskName(mask);
        }
    }
    if (!unrendered.empty()) {
        // Some devices accept an ambisonic output stream but cannot render it to the current
        // output device. That is a platform limitation, not an Oboe failure.
        GTEST_SKIP() << "Device did not render ambisonic output for:" << unrendered;
    }
}

INSTANTIATE_TEST_SUITE_P(
        DataPaths, StreamAcnMaskData,
        ::testing::Combine(::testing::Values(Direction::Output, Direction::Input),
                           ::testing::Bool(), ::testing::Bool()),
        &StreamAcnMaskData::getTestName);

TEST_F(StreamAcnMask, ReopenAfterClose) {
    if (!isSupported()) {
        GTEST_SKIP() << "Ambisonic channel masks are not supported";
    }
    mBuilder.setDirection(Direction::Input)->setChannelAcnMask(ChannelAcnMask::Order1);
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(Result::OK, openStream());
        checkOpenedAcnStream(ChannelAcnMask::Order1);
        EXPECT_EQ(Result::OK, mStream->requestStart());
        usleep(50 * 1000);
        EXPECT_EQ(Result::OK, mStream->requestStop());
        closeStream();
    }
}
