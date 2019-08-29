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

#ifndef OBOETESTER_LATENCY_ANALYSER_H
#define OBOETESTER_LATENCY_ANALYSER_H

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

#define LOOPBACK_RESULT_TAG  "RESULT: "

constexpr int32_t kDefaultSampleRate = 48000;
constexpr int32_t kMillisPerSecond   = 1000;

constexpr int32_t kMaxLatencyMillis  = 700;  // arbitrary and generous
constexpr double  kMinimumConfidence = 0.2;

class PseudoRandom {
public:
    PseudoRandom() {}
    PseudoRandom(int64_t seed)
            :    mSeed(seed)
    {}

    /**
     * Returns the next random double from -1.0 to 1.0
     *
     * @return value from -1.0 to 1.0
     */
     double nextRandomDouble() {
        return nextRandomInteger() * (0.5 / (((int32_t)1) << 30));
    }

    /** Calculate random 32 bit number using linear-congruential method.
     */
    int32_t nextRandomInteger() {
#if __has_builtin(__builtin_mul_overflow) && __has_builtin(__builtin_add_overflow)
        int64_t prod;
        // Use values for 64-bit sequence from MMIX by Donald Knuth.
        __builtin_mul_overflow(mSeed, (int64_t)6364136223846793005, &prod);
        __builtin_add_overflow(prod, (int64_t)1442695040888963407, &mSeed);
#else
        mSeed = (mSeed * (int64_t)6364136223846793005) + (int64_t)1442695040888963407;
#endif
        return (int32_t) (mSeed >> 32); // The higher bits have a longer sequence.
    }

private:
    int64_t mSeed = 99887766;
};

class PeakFollower {
public:
    float process(float input) {
        mPrevious *= kDecayCoefficient;
        const float positive = abs(input);
        if (positive > mPrevious) {
            mPrevious = positive;
        }
        return mPrevious;
    }

    float get() {
        return mPrevious;
    }

private:
    static constexpr float kDecayCoefficient = 0.99f;
    float mPrevious = 0.0f;
};

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

/*
    int save(const char *fileName, bool writeShorts = true) {
        SNDFILE *sndFile = nullptr;
        int written = 0;
        SF_INFO info = {
                .frames = mFrameCounter,
                .samplerate = mSampleRate,
                .channels = 1,
                .format = SF_FORMAT_WAV | (writeShorts ? SF_FORMAT_PCM_16 : SF_FORMAT_FLOAT)
        };

        sndFile = sf_open(fileName, SFM_WRITE, &info);
        if (sndFile == nullptr) {
            LOGD("AudioRecording::save(%s) failed to open file\n", fileName);
            return -errno;
        }

        written = sf_writef_float(sndFile, mData, mFrameCounter);

        sf_close(sndFile);
        return written;
    }

    int load(const char *fileName) {
        SNDFILE *sndFile = nullptr;
        SF_INFO info;

        sndFile = sf_open(fileName, SFM_READ, &info);
        if (sndFile == nullptr) {
            LOGD("AudioRecording::load(%s) failed to open file\n", fileName);
            return -errno;
        }

        assert(info.channels == 1);
        assert(info.format == SF_FORMAT_FLOAT);

        setSampleRate(info.samplerate);
        allocate(info.frames);
        mFrameCounter = sf_readf_float(sndFile, mData, info.frames);

        sf_close(sndFile);
        return mFrameCounter;
    }
*/
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

    virtual void reset() {
        mResult = 0;
        mResetCount++;
    }

    virtual result_code process(float *inputData, int inputChannelCount,
                 float *outputData, int outputChannelCount,
                 int numFrames) = 0;


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

protected:
    int32_t   mResetCount = 0;

private:
    int32_t mSampleRate = kDefaultSampleRate;
    int32_t mResult = 0;
};

/*
class PeakAnalyzer {
public:
    float process(float input) {
        float output = mPrevious * mDecay;
        if (input > output) {
            output = input;
        }
        mPrevious = output;
        return output;
    }

private:
    float  mDecay = 0.99f;
    float  mPrevious = 0.0f;
};
*/

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

        LOGD("state reset to STATE_INITIAL_SILENCE");
        mState = STATE_INITIAL_SILENCE;
        mAudioRecording.clear();
        mLatencyReport.reset();
    }

    bool hasEnoughData() {
        return mAudioRecording.isFull();
    }

    bool isDone() override {
        return mState == STATE_DONE || mState == STATE_FAILED;
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

    result_code processOneFrame(float *inputData, int inputChannelCount,
                                float *outputData, int outputChannelCount) {
        echo_state nextState = mState;

        switch (mState) {
            case STATE_INITIAL_SILENCE:
                // Output silence at the beginning.
                for (int i = 0; i < outputChannelCount; i++) {
                    outputData[i] = 0;
                }
                // Measure background RMS on channel 0
                mBackgroundSumSquare += inputData[0] * inputData[0];
                mBackgroundSumCount++;

                mDownCounter--;
                if (mDownCounter <= 0) {
                    mBackgroundRMS = sqrtf(mBackgroundSumSquare / mBackgroundSumCount);
                    nextState = STATE_SENDING_PULSE;
                    mPulseCursor = 0;
                    LOGD("LatencyAnalyzer state => STATE_SENDING_PULSE");
                }
                break;

            case STATE_SENDING_PULSE:
                {
                    float pulseSample = mPulse.getData()[mPulseCursor++];
                    for (int i = 0; i < outputChannelCount; i++) {
                        outputData[i] = pulseSample;
                    }
                }
                mAudioRecording.write(inputData, inputChannelCount, 1);
                if (hasEnoughData()) {
                    LOGD("LatencyAnalyzer state => STATE_GOT_DATA");
                    nextState = STATE_GOT_DATA;
                } else if (mPulseCursor >= mPulse.size()) {
                    LOGD("LatencyAnalyzer state => STATE_GATHERING_ECHOS");
                    nextState = STATE_GATHERING_ECHOS;
                }
                break;

            case STATE_GATHERING_ECHOS:
                // Record input until the mAudioRecording is full.
                mAudioRecording.write(inputData, inputChannelCount, 1);
                if (hasEnoughData()) {
                    LOGD("LatencyAnalyzer state => STATE_GOT_DATA");
                    nextState = STATE_GOT_DATA;
                }
                for (int i = 0; i < outputChannelCount; i++) {
                    outputData[i] = 0.0f; // silence
                }
                break;

            case STATE_GOT_DATA:
            case STATE_DONE:
            case STATE_FAILED:
            default:
                for (int i = 0; i < outputChannelCount; i++) {
                    outputData[i] = 0.0f; // silence
                }
                break;
        }

        mState = nextState;
        return RESULT_OK;
    }

    result_code process(float *inputData, int inputChannelCount,
                        float *outputData, int outputChannelCount,
                        int numFrames) override {
        result_code result = RESULT_OK;
        mLoopCounter++;
        // Process one frame at a time.
        for (int i = 0; i < numFrames; i++) {
            result = processOneFrame(inputData, inputChannelCount, outputData, outputChannelCount);
            inputData += inputChannelCount;
            outputData += outputChannelCount;
            if (result != RESULT_OK) {
                break;
            }
        }
        return result;
    }
/*
    int save(const char *fileName) override {
        return mAudioRecording.save(fileName);
    }

    int load(const char *fileName) override {
        int result = mAudioRecording.load(fileName);
        setSampleRate(mAudioRecording.getSampleRate());
        mState = STATE_DONE;
        return result;
    }
*/
private:

    enum echo_state {
        STATE_INITIAL_SILENCE,
        STATE_SENDING_PULSE,
        STATE_GATHERING_ECHOS,
        STATE_GOT_DATA, // must match RoundTripLatencyActivity.java
        STATE_DONE,
        STATE_FAILED
    };

    const char *convertStateToText(echo_state state) {
        const char *result = "Unknown";
        switch(state) {
            case STATE_INITIAL_SILENCE:
                result = "INIT";
                break;
            case STATE_SENDING_PULSE:
                result = "PULSE";
                break;
            case STATE_GATHERING_ECHOS:
                result = "ECHOS";
                break;
            case STATE_GOT_DATA:
                result = "GOT_DATA";
                break;
            case STATE_DONE:
                result = "DONE";
                break;
            case STATE_FAILED:
                result = "FAILED";
                break;
        }
        return result;
    }

    int32_t         mDownCounter = 500;
    int32_t         mLoopCounter = 0;
    echo_state      mState = STATE_INITIAL_SILENCE;

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

// ====================================================================================
/**
 * Output a steady sinewave and analyze the return signal.
 *
 * Use a cosine transform to measure the predicted magnitude and relative phase of the
 * looped back sine wave. Then generate a predicted signal and compare with the actual signal.
 */
class GlitchAnalyzer : public LoopbackProcessor {
public:

    int32_t getState() {
        return mState;
    }

    float getPeakAmplitude() {
        return mPeakFollower.get();
    }

    int32_t getGlitchCount() {
        return mGlitchCount;
    }

    double getSignalToNoiseDB() {
        static const double threshold = 1.0e-14;
        if (mMeanSquareSignal < threshold || mMeanSquareNoise < threshold) {
            return 0.0;
        } else {
            double signalToNoise = mMeanSquareSignal / mMeanSquareNoise; // power ratio
            double signalToNoiseDB = 10.0 * log(signalToNoise);
            if (signalToNoiseDB < MIN_SNRATIO_DB) {
                LOGD("ERROR - signal to noise ratio is too low! < %d dB. Adjust volume.",
                     MIN_SNRATIO_DB);
                setResult(ERROR_VOLUME_TOO_LOW);
            }
            return signalToNoiseDB;
        }
    }

    void analyze() override {
        LOGD("GlitchAnalyzer ------------------");
        LOGD(LOOPBACK_RESULT_TAG "peak.amplitude     = %8f", getPeakAmplitude());
        LOGD(LOOPBACK_RESULT_TAG "sine.magnitude     = %8f", mMagnitude);
        LOGD(LOOPBACK_RESULT_TAG "rms.noise          = %8f", mMeanSquareNoise);
        LOGD(LOOPBACK_RESULT_TAG "signal.to.noise.db = %8.2f", getSignalToNoiseDB());
        LOGD(LOOPBACK_RESULT_TAG "frames.accumulated = %8d", mFramesAccumulated);
        LOGD(LOOPBACK_RESULT_TAG "sine.period        = %8d", mSinePeriod);
        LOGD(LOOPBACK_RESULT_TAG "test.state         = %8d", mState);
        LOGD(LOOPBACK_RESULT_TAG "frame.count        = %8d", mFrameCounter);
        // Did we ever get a lock?
        bool gotLock = (mState == STATE_LOCKED) || (mGlitchCount > 0);
        if (!gotLock) {
            LOGD("ERROR - failed to lock on reference sine tone");
            setResult(ERROR_NO_LOCK);
        } else {
            // Only print if meaningful.
            LOGD(LOOPBACK_RESULT_TAG "glitch.count       = %8d", mGlitchCount);
            LOGD(LOOPBACK_RESULT_TAG "max.glitch         = %8f", mMaxGlitchDelta);
            if (mGlitchCount > 0) {
                LOGD("ERROR - number of glitches > 0");
                setResult(ERROR_GLITCHES);
            }
        }
    }

    void printStatus() override {
        LOGD("st = %d, #gl = %3d,", mState, mGlitchCount);
    }

    double calculateMagnitude(double *phasePtr = NULL) {
        if (mFramesAccumulated == 0) {
            return 0.0;
        }
        double sinMean = mSinAccumulator / mFramesAccumulated;
        double cosMean = mCosAccumulator / mFramesAccumulated;
        double magnitude = 2.0 * sqrt( (sinMean * sinMean) + (cosMean * cosMean ));
        if( phasePtr != NULL )
        {
            double phase = M_PI_2 - atan2( sinMean, cosMean );
            *phasePtr = phase;
        }
        return magnitude;
    }

    /**
     * @param inputData contains microphone data with sine signal feedback
     * @param outputData contains the reference sine wave
     */
    result_code processOneFrame(float *inputData, int inputChannelCount,
                                float *outputData, int outputChannelCount) {
        result_code result = RESULT_OK;
        bool sineEnabled = true;

        float peak = mPeakFollower.process(*inputData);

        float sample = inputData[0];
        float sinOut = sinf(mPhase);

        switch (mState) {
            case STATE_IDLE:
                sineEnabled = false;
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mState = STATE_WAITING_FOR_SIGNAL;
                    mDownCounter = NOISE_FRAME_COUNT;
                    mPhase = 0.0; // prevent spike at start
                }
                break;

            case STATE_IMMUNE:
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mState = STATE_WAITING_FOR_SIGNAL;
                }
                break;

            case STATE_WAITING_FOR_SIGNAL:
                if (peak > mThreshold) {
                    mState = STATE_WAITING_FOR_LOCK;
                    //LOGD("%5d: switch to STATE_WAITING_FOR_LOCK", mFrameCounter);
                    resetAccumulator();
                }
                break;

            case STATE_WAITING_FOR_LOCK:
                mSinAccumulator += sample * sinOut;
                mCosAccumulator += sample * cosf(mPhase);
                mFramesAccumulated++;
                // Must be a multiple of the period or the calculation will not be accurate.
                if (mFramesAccumulated == mSinePeriod * PERIODS_NEEDED_FOR_LOCK) {
                    mPhaseOffset = 0.0;
                    mMagnitude = calculateMagnitude(&mPhaseOffset);
                    if (mMagnitude > mThreshold) {
                        if (fabs(mPreviousPhaseOffset - mPhaseOffset) < 0.001) {
                            mState = STATE_LOCKED;
                            mScaledTolerance = mMagnitude * kTolerance;
                            //LOGD("%5d: switch to STATE_LOCKED", mFrameCounter);
                        }
                        mPreviousPhaseOffset = mPhaseOffset;
                    }
                    resetAccumulator();
                }
                break;

            case STATE_LOCKED: {
                // Predict next sine value
                float predicted = sinf(mPhase + mPhaseOffset) * mMagnitude;
                // LOGD("    predicted = %f, actual = %f", predicted, sample);

                float diff = predicted - sample;
                mSumSquareSignal += predicted * predicted;
                mSumSquareNoise += diff * diff;
                float absDiff = fabs(diff);
                mMaxGlitchDelta = std::max(mMaxGlitchDelta, absDiff);
                if (absDiff > mScaledTolerance) {
                    result = ERROR_GLITCHES;
                    addGlitch();
                } else {
                    // Track incoming signal and slowly adjust magnitude to account
                    // for drift in the DRC or AGC.
                    mSinAccumulator += sample * sinOut;
                    mCosAccumulator += sample * cosf(mPhase);
                    mFramesAccumulated++;
                    // Must be a multiple of the period or the calculation will not be accurate.
                    if (mFramesAccumulated == mSinePeriod) {
                        const double coefficient = 0.1;
                        double phaseOffset = 0.0;
                        double magnitude = calculateMagnitude(&phaseOffset);
                        // One pole averaging filter.
                        mMagnitude = (mMagnitude * (1.0 - coefficient)) + (magnitude * coefficient);
                        mScaledTolerance = mMagnitude * kTolerance;

                        mMeanSquareNoise = mSumSquareNoise * mInverseSinePeriod;
                        mMeanSquareSignal = mSumSquareSignal * mInverseSinePeriod;
                        resetAccumulator();

                        if (mMagnitude < mThreshold) {
                            result = ERROR_GLITCHES;
                            addGlitch();
                        }
                    }
                }
            } break;
        }

        float output = 0.0f;
        // Output sine wave so we can measure it.
        if (sineEnabled) {
            output = (sinOut * mOutputAmplitude)
                     + (mWhiteNoise.nextRandomDouble() * kNoiseAmplitude);
            // LOGD("%5d: sin(%f) = %f, %f", i, mPhase, sinOut,  mPhaseIncrement);
            // advance and wrap phase
            mPhase += mPhaseIncrement;
            if (mPhase > M_PI) {
                mPhase -= (2.0 * M_PI);
            }
        }
        outputData[0] = output;
        mFrameCounter++;

        return result;
    }

    result_code process(float *inputData, int inputChannelCount,
                        float *outputData, int outputChannelCount,
                        int numFrames) override {
        result_code result = RESULT_OK;
        // Process one frame at a time.
        for (int i = 0; i < numFrames; i++) {
            result_code tempResult = processOneFrame(inputData, inputChannelCount, outputData,
                                                     outputChannelCount);
            if (tempResult != RESULT_OK) {
                result = tempResult;
            }
            inputData += inputChannelCount;
            outputData += outputChannelCount;
        }
        return result;
    }

    void addGlitch() {
        mGlitchCount++;
//        LOGD("%5d: Got a glitch # %d, predicted = %f, actual = %f",
//        mFrameCounter, mGlitchCount, predicted, sample);
        mState = STATE_IMMUNE;
        mDownCounter = mSinePeriod * PERIODS_IMMUNE;
        resetAccumulator();
    }

    void resetAccumulator() {
        mFramesAccumulated = 0;
        mSinAccumulator = 0.0;
        mCosAccumulator = 0.0;
        mSumSquareSignal = 0.0;
        mSumSquareNoise = 0.0;
    }

    void reset() override {
        LoopbackProcessor::reset();
        mGlitchCount = 0;
        mState = STATE_IDLE;
        mDownCounter = IDLE_FRAME_COUNT;
        mSinePeriod = getSampleRate() / kTargetGlitchFrequency;
        mInverseSinePeriod = 1.0 / mSinePeriod;
        mPhaseIncrement = 2.0 * M_PI * mInverseSinePeriod;
        mPhase = 0.0f;
        mMaxGlitchDelta = 0.0;
        resetAccumulator();
    }

private:

    // These must match the values in GlitchActivity.java
    enum sine_state_t {
        STATE_IDLE,
        STATE_IMMUNE,
        STATE_WAITING_FOR_SIGNAL,
        STATE_WAITING_FOR_LOCK,
        STATE_LOCKED
    };

    enum constants {
        // Arbitrary durations, assuming 48000 Hz
        IDLE_FRAME_COUNT = 48 * 100,
        NOISE_FRAME_COUNT = 48 * 600,
        PERIODS_NEEDED_FOR_LOCK = 8,
        PERIODS_IMMUNE = 2,
        MIN_SNRATIO_DB = 65
    };

    static constexpr float kTolerance = 0.10; // scale from 0.0 to 1.0
    static constexpr float kNoiseAmplitude = 0.00; // Used to experiment with warbling caused by DRC.
    static constexpr int kTargetGlitchFrequency = 607;
    double  mThreshold = 0.005;
    int     mSinePeriod = 1; // this will be set before use
    double  mInverseSinePeriod = 1.0;

    double  mPhaseIncrement = 0.0;
    double  mPhase = 0.0;
    double  mPhaseOffset = 0.0;
    double  mPreviousPhaseOffset = 0.0;
    double  mMagnitude = 0.0;
    int32_t mFramesAccumulated = 0;
    double  mSinAccumulator = 0.0;
    double  mCosAccumulator = 0.0;
    float   mMaxGlitchDelta = 0.0f;
    int32_t mGlitchCount = 0;
    float   mScaledTolerance = 0.0;
    int     mDownCounter = IDLE_FRAME_COUNT;
    int32_t mFrameCounter = 0;
    float   mOutputAmplitude = 0.75;

    // measure background noise continuously as a deviation from the expected signal
    double  mSumSquareSignal = 0.0;
    double  mSumSquareNoise = 0.0;
    double  mMeanSquareSignal = 0.0;
    double  mMeanSquareNoise = 0.0;

    PeakFollower  mPeakFollower;

    PseudoRandom  mWhiteNoise;

    sine_state_t  mState = STATE_IDLE;
};

#undef LOOPBACK_RESULT_TAG

#endif // OBOETESTER_LATENCY_ANALYSER_H
