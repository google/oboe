/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * Tools for measuring latency and for detecting glitches.
 * These classes are pure math and can be used with any audio system.
 */

#ifndef ANALYZER_LATENCY_ANALYZER_H
#define ANALYZER_LATENCY_ANALYZER_H

#include <algorithm>
#include <assert.h>
#include <cctype>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include "RandomPulseGenerator.h"
#include "PeakDetector.h"
#include "PseudoRandom.h"

#define LOOPBACK_RESULT_TAG  "RESULT: "

static constexpr int32_t kDefaultSampleRate = 48000;
static constexpr int32_t kMillisPerSecond   = 1000;
static constexpr int32_t kMaxLatencyMillis  = 700;  // arbitrary and generous
static constexpr double  kMinimumConfidence = 0.2;

typedef struct LatencyReport_s {
    int32_t latencyInFrames = 0.0;
    double confidence = 0.0;

    void reset() {
        latencyInFrames = 0;
        confidence = 0.0;
    }
} LatencyReport;

// Calculate a normalized cross correlation.
static double calculateNormalizedCorrelation(const float *a,
                                             const float *b,
                                             int windowSize)
{
    double correlation = 0.0;
    double sumProducts = 0.0;
    double sumSquares = 0.0;

    // Correlate a against b.
    for (int i = 0; i < windowSize; i++) {
        float s1 = a[i];
        float s2 = b[i];
        // Use a normalized cross-correlation.
        sumProducts += s1 * s2;
        sumSquares += ((s1 * s1) + (s2 * s2));
    }

    if (sumSquares >= 1.0e-9) {
        correlation = (float) (2.0 * sumProducts / sumSquares);
    }
    return correlation;
}

static double calculateRootMeanSquare(float *data, int32_t numSamples) {
    double sum = 0.0;
    for (int32_t i = 0; i < numSamples; i++) {
        float sample = data[i];
        sum += sample * sample;
    }
    return sqrt(sum / numSamples);
}

/**
 * Monophonic recording with processing.
 */
class AudioRecording
{
public:
    AudioRecording() {
    }
    ~AudioRecording() {
        delete[] mData;
    }

    void allocate(int maxFrames) {
        delete[] mData;
        mData = new float[maxFrames];
        mMaxFrames = maxFrames;
    }

    // Write SHORT data from the first channel.
    int32_t write(int16_t *inputData, int32_t inputChannelCount, int32_t numFrames) {
        // stop at end of buffer
        if ((mFrameCounter + numFrames) > mMaxFrames) {
            numFrames = mMaxFrames - mFrameCounter;
        }
        for (int i = 0; i < numFrames; i++) {
            mData[mFrameCounter++] = inputData[i * inputChannelCount] * (1.0f / 32768);
        }
        return numFrames;
    }

    // Write FLOAT data from the first channel.
    int32_t write(float *inputData, int32_t inputChannelCount, int32_t numFrames) {
        // stop at end of buffer
        if ((mFrameCounter + numFrames) > mMaxFrames) {
            numFrames = mMaxFrames - mFrameCounter;
        }
        for (int i = 0; i < numFrames; i++) {
            mData[mFrameCounter++] = inputData[i * inputChannelCount];
        }
        return numFrames;
    }

    // Write FLOAT data from the first channel.
    int32_t write(float sample) {
        // stop at end of buffer
        if (mFrameCounter < mMaxFrames) {
            mData[mFrameCounter++] = sample;
        }
        return 1;
    }

    void clear() {
        mFrameCounter = 0;
    }
    int32_t size() {
        return mFrameCounter;
    }

    bool isFull() {
        return mFrameCounter >= mMaxFrames;
    }

    float *getData() {
        return mData;
    }

    void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
    }

    int32_t getSampleRate() {
        return mSampleRate;
    }

    /**
     * Square the samples so they are all positive and so the peaks are emphasized.
     */
    void square() {
        for (int i = 0; i < mFrameCounter; i++) {
            const float sample = mData[i];
            mData[i] = sample * sample;
        }
    }

    /**
     * Amplify a signal so that the peak matches the specified target.
     *
     * @param target final max value
     * @return gain applied to signal
     */
    float normalize(float target) {
        float maxValue = 1.0e-9f;
        for (int i = 0; i < mFrameCounter; i++) {
            maxValue = std::max(maxValue, abs(mData[i]));
        }
        float gain = target / maxValue;
        for (int i = 0; i < mFrameCounter; i++) {
            mData[i] *= gain;
        }
        return gain;
    }

private:
    float        *mData = nullptr;
    int32_t       mFrameCounter = 0;
    int32_t       mMaxFrames = 0;
    int32_t       mSampleRate = kDefaultSampleRate; // common default
};

static int measureLatencyFromPulse(AudioRecording &recorded,
                                   AudioRecording &pulse,
                                   int32_t framesPerEncodedBit,
                                   LatencyReport *report) {

    report->latencyInFrames = 0;
    report->confidence = 0.0;

    int numCorrelations = recorded.size() - pulse.size();
    if (numCorrelations < 10) {
        LOGE("%s() recording too small = %d frames", __func__, recorded.size());
        return -1;
    }
    std::unique_ptr<float[]> correlations= std::make_unique<float[]>(numCorrelations);

    // Correlate pulse against the recorded data.
    for (int i = 0; i < numCorrelations; i++) {
        float correlation = (float) calculateNormalizedCorrelation(&recorded.getData()[i],
                                                                   &pulse.getData()[0],
                                                                   pulse.size());
        correlations[i] = correlation;
    }

    // Find highest peak in correlation array.
    float peakCorrelation = 0.0;
    int peakIndex = -1;
    for (int i = 0; i < numCorrelations; i++) {
        float value = abs(correlations[i]);
        if (value > peakCorrelation) {
            peakCorrelation = value;
            peakIndex = i;
        }
    }
    if (peakIndex < 0) {
        LOGE("%s() no signal for correlation", __func__);
        return -2;
    }

    report->latencyInFrames = peakIndex;
    report->confidence = peakCorrelation;

    return 0;
}

// ====================================================================================
class LoopbackProcessor {
public:
    virtual ~LoopbackProcessor() = default;

    // Note that these values must match the switch in RoundTripLatencyActivity.h
    enum result_code {
        RESULT_OK = 0,
        ERROR_NOISY = -99,
        ERROR_VOLUME_TOO_LOW,
        ERROR_VOLUME_TOO_HIGH,
        ERROR_CONFIDENCE,
        ERROR_INVALID_STATE,
        ERROR_GLITCHES,
        ERROR_NO_LOCK
    };

    virtual void onStartTest() {
        reset();
    }

    virtual void reset() {
        mResult = 0;
        mResetCount++;
    }

    virtual result_code processInputFrame(float *frameData, int channelCount) = 0;
    virtual result_code processOutputFrame(float *frameData, int channelCount) = 0;

    void process(float *inputData, int inputChannelCount, int numInputFrames,
                        float *outputData, int outputChannelCount, int numOutputFrames) {
        int numBoth = std::min(numInputFrames, numOutputFrames);
        // Process one frame at a time.
        for (int i = 0; i < numBoth; i++) {
            processInputFrame(inputData, inputChannelCount);
            inputData += inputChannelCount;
            processOutputFrame(outputData, outputChannelCount);
            outputData += outputChannelCount;
        }
        // If there is more input than output.
        for (int i = numBoth; i < numInputFrames; i++) {
            processInputFrame(inputData, inputChannelCount);
            inputData += inputChannelCount;
        }
        // If there is more output than input.
        for (int i = numBoth; i < numOutputFrames; i++) {
            processOutputFrame(outputData, outputChannelCount);
            outputData += outputChannelCount;
        }
    }

    virtual void analyze() = 0;

    virtual void printStatus() {};

    int32_t getResult() {
        return mResult;
    }

    void setResult(int32_t result) {
        mResult = result;
    }

    virtual bool isDone() {
        return false;
    }

    virtual int save(const char *fileName) {
        (void) fileName;
        return -1;
    }

    virtual int load(const char *fileName) {
        (void) fileName;
        return -1;
    }

    virtual void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
    }

    int32_t getSampleRate() {
        return mSampleRate;
    }

    int32_t getResetCount() {
        return mResetCount;
    }

    /** Called when not enough input frames could be read after synchronization.
     */
    virtual void onInsufficientRead() {
        reset();
    }

protected:
    int32_t   mResetCount = 0;

private:
    int32_t mSampleRate = kDefaultSampleRate;
    int32_t mResult = 0;
};

class LatencyAnalyzer : public LoopbackProcessor {
public:

    LatencyAnalyzer() : LoopbackProcessor() {}
    virtual ~LatencyAnalyzer() = default;

    virtual int32_t getProgress() = 0;

    virtual int getState() = 0;

    // @return latency in frames
    virtual int32_t getMeasuredLatency() = 0;

    virtual double getMeasuredConfidence() = 0;

    virtual double getBackgroundRMS() = 0;

    virtual double getSignalRMS() = 0;

};

// ====================================================================================
/**
 * Measure latency given a loopback stream data.
 * Use an encoded bit train as the sound source because it
 * has an unambiguous correlation value.
 * Uses a state machine to cycle through various stages.
 *
 */
class PulseLatencyAnalyzer : public LatencyAnalyzer {
public:

    PulseLatencyAnalyzer() : LatencyAnalyzer() {
        int32_t maxLatencyFrames = getSampleRate() * kMaxLatencyMillis / kMillisPerSecond;
        int32_t numPulseBits = getSampleRate() * kPulseLengthMillis
                / (kFramesPerEncodedBit * kMillisPerSecond);
        int32_t  pulseLength = numPulseBits * kFramesPerEncodedBit;
        mFramesToRecord = pulseLength + maxLatencyFrames;
        LOGD("PulseLatencyAnalyzer: allocate recording with %d frames", mFramesToRecord);
        mAudioRecording.allocate(mFramesToRecord);
        mAudioRecording.setSampleRate(getSampleRate());
        generateRandomPulse(pulseLength);
    }

    void generateRandomPulse(int32_t pulseLength) {
        mPulse.allocate(pulseLength);
        RandomPulseGenerator pulser(kFramesPerEncodedBit);
        for (int i = 0; i < pulseLength; i++) {
            mPulse.write(pulser.nextFloat());
        }
    }

    int getState() override {
        return mState;
    }

    void setSampleRate(int32_t sampleRate) override {
        LoopbackProcessor::setSampleRate(sampleRate);
        mAudioRecording.setSampleRate(sampleRate);
    }

    void reset() override {
        LoopbackProcessor::reset();
        mDownCounter = getSampleRate() / 2;
        mLoopCounter = 0;

        mPulseCursor = 0;
        mBackgroundSumSquare = 0.0f;
        mBackgroundSumCount = 0;
        mBackgroundRMS = 0.0f;
        mSignalRMS = 0.0f;

        LOGD("state reset to STATE_MEASURE_BACKGROUND");
        mState = STATE_MEASURE_BACKGROUND;
        mAudioRecording.clear();
        mLatencyReport.reset();
    }

    bool hasEnoughData() {
        return mAudioRecording.isFull();
    }

    bool isDone() override {
        return mState == STATE_DONE;
    }

    int32_t getProgress() override {
        return mAudioRecording.size();
    }

    void analyze() override {
        LOGD("PulseLatencyAnalyzer ---------------");
        LOGD(LOOPBACK_RESULT_TAG "test.state             = %8d", mState);
        LOGD(LOOPBACK_RESULT_TAG "test.state.name        = %8s", convertStateToText(mState));
        LOGD(LOOPBACK_RESULT_TAG "background.rms         = %8f", mBackgroundRMS);

        int32_t newResult = RESULT_OK;
        if (mState != STATE_GOT_DATA) {
            LOGD("WARNING - Bad state. Check volume on device.");
            // setResult(ERROR_INVALID_STATE);
        } else {
            LOGD("Please wait several seconds for cross-correlation to complete.");
            float gain = mAudioRecording.normalize(1.0f);
            measureLatencyFromPulse(mAudioRecording,
                                    mPulse,
                                    kFramesPerEncodedBit,
                                    &mLatencyReport);

            if (mLatencyReport.confidence < kMinimumConfidence) {
                LOGD("   ERROR - confidence too low!");
                newResult = ERROR_CONFIDENCE;
            } else {
                mSignalRMS = calculateRootMeanSquare(
                        &mAudioRecording.getData()[mLatencyReport.latencyInFrames], mPulse.size())
                                / gain;
            }
#if OBOE_ENABLE_LOGGING
            double latencyMillis = kMillisPerSecond * (double) mLatencyReport.latencyInFrames
                                   / getSampleRate();
#endif
            LOGD(LOOPBACK_RESULT_TAG "latency.frames         = %8d",
                   mLatencyReport.latencyInFrames);
            LOGD(LOOPBACK_RESULT_TAG "latency.msec           = %8.2f",
                   latencyMillis);
            LOGD(LOOPBACK_RESULT_TAG "latency.confidence     = %8.6f",
                   mLatencyReport.confidence);
        }
        mState = STATE_DONE;
        if (getResult() == RESULT_OK) {
            setResult(newResult);
        }
    }

    int32_t getMeasuredLatency() override {
        return mLatencyReport.latencyInFrames;
    }

    double getMeasuredConfidence() override {
        return mLatencyReport.confidence;
    }

    double getBackgroundRMS() override {
        return mBackgroundRMS;
    }

    double getSignalRMS() override {
        return mSignalRMS;
    }

    void printStatus() override {
        LOGD("st = %d", mState);
    }

    result_code processInputFrame(float *frameData, int channelCount) override {
        echo_state nextState = mState;
        mLoopCounter++;

        switch (mState) {
            case STATE_MEASURE_BACKGROUND:
                // Measure background RMS on channel 0
                mBackgroundSumSquare += frameData[0] * frameData[0];
                mBackgroundSumCount++;
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mBackgroundRMS = sqrtf(mBackgroundSumSquare / mBackgroundSumCount);
                    nextState = STATE_IN_PULSE;
                    mPulseCursor = 0;
                    LOGD("LatencyAnalyzer state => STATE_SENDING_PULSE");
                }
                break;

            case STATE_IN_PULSE:
                // Record input until the mAudioRecording is full.
                mAudioRecording.write(frameData, channelCount, 1);
                if (hasEnoughData()) {
                    LOGD("LatencyAnalyzer state => STATE_GOT_DATA");
                    nextState = STATE_GOT_DATA;
                }
                break;

            case STATE_GOT_DATA:
            case STATE_DONE:
            default:
                break;
        }

        mState = nextState;
        return RESULT_OK;
    }

    result_code processOutputFrame(float *frameData, int channelCount) override {
        switch (mState) {
            case STATE_IN_PULSE:
                if (mPulseCursor < mPulse.size()) {
                    float pulseSample = mPulse.getData()[mPulseCursor++];
                    for (int i = 0; i < channelCount; i++) {
                        frameData[i] = pulseSample;
                    }
                } else {
                    for (int i = 0; i < channelCount; i++) {
                        frameData[i] = 0;
                    }
                }
                break;

            case STATE_MEASURE_BACKGROUND:
            case STATE_GOT_DATA:
            case STATE_DONE:
            default:
                for (int i = 0; i < channelCount; i++) {
                    frameData[i] = 0.0f; // silence
                }
                break;
        }

        return RESULT_OK;
    }

private:

    enum echo_state {
        STATE_MEASURE_BACKGROUND,
        STATE_IN_PULSE,
        STATE_GOT_DATA, // must match RoundTripLatencyActivity.java
        STATE_DONE,
    };

    const char *convertStateToText(echo_state state) {
        const char *result = "Unknown";
        switch(state) {
            case STATE_MEASURE_BACKGROUND:
                result = "INIT";
                break;
            case STATE_IN_PULSE:
                result = "PULSE";
                break;
            case STATE_GOT_DATA:
                result = "GOT_DATA";
                break;
            case STATE_DONE:
                result = "DONE";
                break;
        }
        return result;
    }

    int32_t         mDownCounter = 500;
    int32_t         mLoopCounter = 0;
    echo_state      mState = STATE_MEASURE_BACKGROUND;

    static constexpr int32_t kFramesPerEncodedBit = 8; // multiple of 2
    static constexpr int32_t kPulseLengthMillis = 500;

    AudioRecording     mPulse;
    int32_t            mPulseCursor = 0;

    float              mBackgroundSumSquare = 0.0f;
    int32_t            mBackgroundSumCount = 0;
    float              mBackgroundRMS = 0.0f;
    float              mSignalRMS = 0.0f;
    int32_t            mFramesToRecord = 0;

    AudioRecording     mAudioRecording; // contains only the input after starting the pulse
    LatencyReport      mLatencyReport;
};

#endif // ANALYZER_LATENCY_ANALYZER_H
