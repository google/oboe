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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LOOPBACK_RESULT_TAG  "RESULT: "

constexpr int32_t kDefaultSampleRate = 48000;
constexpr int32_t kMillisPerSecond   = 1000;
constexpr int32_t kMinLatencyMillis  = 4;    // arbitrary and very low
constexpr int32_t kMaxLatencyMillis  = 400;  // arbitrary and generous
constexpr double  kMaxEchoGain       = 10.0; // based on experiments, otherwise too noisy
constexpr double  kMinimumConfidence = 0.5;

/*

FIR filter designed with
http://t-filter.appspot.com

sampling frequency: 48000 Hz

* 0 Hz - 8000 Hz
  gain = 1.2
  desired ripple = 5 dB
  actual ripple = 5.595266169703693 dB

* 12000 Hz - 20000 Hz
  gain = 0
  desired attenuation = -40 dB
  actual attenuation = -37.58691566571914 dB

*/

#define FILTER_TAP_NUM 11

static const float sFilterTaps8000[FILTER_TAP_NUM] = {
        -0.05944219353343189f,
        -0.07303434839503208f,
        -0.037690487672689066f,
         0.1870480506596512f,
         0.3910337357836833f,
         0.5333672385425637f,
         0.3910337357836833f,
         0.1870480506596512f,
        -0.037690487672689066f,
        -0.07303434839503208f,
        -0.05944219353343189f
};

class LowPassFilter {
public:

    /*
     * Filter one input sample.
     * @return filtered output
     */
    float filter(float input) {
        float output = 0.0f;
        mX[mCursor] = input;
        // Index backwards over x.
        int xIndex = mCursor + FILTER_TAP_NUM;
        // Write twice so we avoid having to wrap in the middle of the convolution.
        mX[xIndex] = input;
        for (int i = 0; i < FILTER_TAP_NUM; i++) {
            output += sFilterTaps8000[i] * mX[xIndex--];
        }
        if (++mCursor >= FILTER_TAP_NUM) {
            mCursor = 0;
        }
        return output;
    }

    /**
     * @return true if PASSED
     */
    bool test() {
        // Measure the impulse of the filter at different phases so we exercise
        // all the wraparound cases in the FIR.
        for (int offset = 0; offset < (FILTER_TAP_NUM * 2); offset++ ) {
            // LOGD("LowPassFilter: cursor = %d\n", mCursor);
            // Offset by one each time.
            if (filter(0.0f) != 0.0f) {
                LOGD("ERROR: filter should return 0.0 before impulse response\n");
                return false;
            }
            for (int i = 0; i < FILTER_TAP_NUM; i++) {
                float output = filter((i == 0) ? 1.0f : 0.0f); // impulse
                if (output != sFilterTaps8000[i]) {
                    LOGD("ERROR: filter should return impulse response\n");
                    return false;
                }
            }
            for (int i = 0; i < FILTER_TAP_NUM; i++) {
                if (filter(0.0f) != 0.0f) {
                    LOGD("ERROR: filter should return 0.0 after impulse response\n");
                    return false;
                }
            }
        }
        return true;
    }

private:
    float   mX[FILTER_TAP_NUM * 2]{}; // twice as big as needed to avoid wrapping
    int32_t mCursor = 0;
};

// A narrow impulse seems to have better immunity against over estimating the
// latency due to detecting subharmonics by the auto-correlator.
static const float s_Impulse[] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.3f, // silence on each side of the impulse
        0.99f, 0.0f, -0.99f, // bipolar with one zero crossing in middle
        -0.3f, 0.0f, 0.0f, 0.0f, 0.0f
};

constexpr int32_t kImpulseSizeInFrames = (int32_t)(sizeof(s_Impulse) / sizeof(s_Impulse[0]));

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

    /** Calculate random 32 bit number using linear-congruential method. */
    int32_t nextRandomInteger() {
        // Use values for 64-bit sequence from MMIX by Donald Knuth.
        mSeed = (mSeed * (int64_t)6364136223846793005) + (int64_t)1442695040888963407;
        return (int32_t) (mSeed >> 32); // The higher bits have a longer sequence.
    }

private:
    int64_t mSeed = 99887766;
};


typedef struct LatencyReport_s {
    double latencyInFrames = 0.0;
    double confidence = 0.0;

    void reset() {
        latencyInFrames = 0.0;
        confidence = 0.0;
    }
} LatencyReport;

static double calculateCorrelation(const float *a,
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

    if (sumSquares >= 0.00000001) {
        correlation = (float) (2.0 * sumProducts / sumSquares);
    }
    return correlation;
}

static int measureLatencyFromEchos(const float *data,
                                   int32_t numFloats,
                                   int32_t sampleRate,
                                   LatencyReport *report) {
    // Allocate results array
    const int minReasonableLatencyFrames = sampleRate * kMinLatencyMillis / kMillisPerSecond;
    const int maxReasonableLatencyFrames = sampleRate * kMaxLatencyMillis / kMillisPerSecond;
    int32_t maxCorrelationSize = maxReasonableLatencyFrames * 3;
    int numCorrelations = std::min(numFloats, maxCorrelationSize);
    float *correlations = new float[numCorrelations]{};
    float *harmonicSums = new float[numCorrelations]{};

    // Perform sliding auto-correlation.
    // Skip first frames to avoid huge peak at zero offset.
    for (int i = minReasonableLatencyFrames; i < numCorrelations; i++) {
        int32_t remaining = numFloats - i;
        float correlation = (float) calculateCorrelation(&data[i], data, remaining);
        correlations[i] = correlation;
        // LOGD("correlation[%d] = %f\n", ic, correlation);
    }

    // Apply a technique similar to Harmonic Product Spectrum Analysis to find echo fundamental.
    // Add higher harmonics mapped onto lower harmonics. This reinforces the "fundamental" echo.
    const int numEchoes = 8;
    for (int partial = 1; partial < numEchoes; partial++) {
        for (int i = minReasonableLatencyFrames; i < numCorrelations; i++) {
            harmonicSums[i / partial] += correlations[i] / partial;
        }
    }

    // Find highest peak in correlation array.
    float maxCorrelation = 0.0;
    int peakIndex = 0;
    for (int i = 0; i < numCorrelations; i++) {
        if (harmonicSums[i] > maxCorrelation) {
            maxCorrelation = harmonicSums[i];
            peakIndex = i;
            // LOGD("maxCorrelation = %f at %d\n", maxCorrelation, peakIndex);
        }
    }
    report->latencyInFrames = peakIndex;
/*
    {
        int32_t topPeak = peakIndex * 7 / 2;
        for (int i = 0; i < topPeak; i++) {
            float sample = harmonicSums[i];
            LOGD("%4d: %7.5f ", i, sample);
            printAudioScope(sample);
        }
    }
*/

    // Calculate confidence.
    if (maxCorrelation < 0.001) {
        report->confidence = 0.0;
    } else {
        // Compare peak to average value around peak.
        int32_t numSamples = std::min(numCorrelations, peakIndex * 2);
        if (numSamples <= 0) {
            report->confidence = 0.0;
        } else {
            double sum = 0.0;
            for (int i = 0; i < numSamples; i++) {
                sum += harmonicSums[i];
            }
            const double average = sum / numSamples;
            const double ratio = average / maxCorrelation; // will be < 1.0
            report->confidence = 1.0 - sqrt(ratio);
        }
    }

    delete[] correlations;
    delete[] harmonicSums;
    return 0;
}

/**
 * Monophonic recording.
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
     * Low pass filter the recording using a simple FIR filter.
     * Note that the lowpass filter cutoff tracks the sample rate.
     * That is OK because the impulse width is a fixed number of samples.
     */
    void lowPassFilter() {
        for (int i = 0; i < mFrameCounter; i++) {
            mData[i] = mLowPassFilter.filter(mData[i]);
        }
    }

    /**
     * Remove DC offset using a one-pole one-zero IIR filter.
     */
    void dcBlocker() {
        const float R = 0.996; // narrow notch at zero Hz
        float x1 = 0.0;
        float y1 = 0.0;
        for (int i = 0; i < mFrameCounter; i++) {
            const float x = mData[i];
            const float y = x - x1 + (R * y1);
            mData[i] = y;
            y1 = y;
            x1 = x;
        }
    }

private:
    float        *mData = nullptr;
    int32_t       mFrameCounter = 0;
    int32_t       mMaxFrames = 0;
    int32_t       mSampleRate = kDefaultSampleRate; // common default
    LowPassFilter mLowPassFilter;
};

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
        mSampleRate = kDefaultSampleRate;
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
        return AAUDIO_ERROR_UNIMPLEMENTED;
    }

    virtual int load(const char *fileName) {
        (void) fileName;
        return AAUDIO_ERROR_UNIMPLEMENTED;
    }

    virtual void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
    }

    int32_t getSampleRate() {
        return mSampleRate;
    }

    // Measure peak amplitude of first channel of buffer.
    static float measurePeakAmplitude(float *inputData, int inputChannelCount, int numFrames) {
        float peak = 0.0f;
        for (int i = 0; i < numFrames; i++) {
            const float pos = fabs(*inputData);
            if (pos > peak) {
                peak = pos;
            }
            inputData += inputChannelCount;
        }
        return peak;
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

    virtual double getMeasuredLatency() = 0;

    virtual double getMeasuredConfidence() = 0;

};

// ====================================================================================
/**
 * Measure latency given a loopback stream data.
 * Use Larsen effect.
 * Uses a state machine to cycle through various stages including:
 *
 */
class EchoAnalyzer : public LatencyAnalyzer {
public:

    EchoAnalyzer() : LatencyAnalyzer() {
        int32_t framesToRecord = 1 * getSampleRate();
        LOGD("EchoAnalyzer: allocate recording with %d frames", framesToRecord);
        mAudioRecording.allocate(framesToRecord);
        mAudioRecording.setSampleRate(getSampleRate());
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
        mMeasuredLoopGain = 0.0f;
        mEchoGain = 1.0f;

        LOGD("state reset to STATE_INITIAL_SILENCE");
        mState = STATE_INITIAL_SILENCE;
        mAudioRecording.clear();
        mLatencyReport.reset();
        mTimeoutCounter = 4 * getSampleRate();
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

//    void setGain(float gain) {
//        mEchoGain = gain;
//    }
//
//    float getGain() {
//        return mEchoGain;
//    }
//
//    bool testLowPassFilter() {
//        LowPassFilter filter;
//        return filter.test();
//    }

    void analyze() override {
        LOGD("EchoAnalyzer ---------------");
        LOGD(LOOPBACK_RESULT_TAG "test.state             = %8d", mState);
        LOGD(LOOPBACK_RESULT_TAG "test.state.name        = %8s", convertStateToText(mState));
        LOGD(LOOPBACK_RESULT_TAG "measured.gain          = %8f", mMeasuredLoopGain);
        LOGD(LOOPBACK_RESULT_TAG "echo.gain              = %8f", mEchoGain);

        int32_t newResult = RESULT_OK;
        if (mState == STATE_WAITING_FOR_SILENCE) {
            LOGW("Stuck waiting for silence. Input may be too noisy!");
            newResult = ERROR_NOISY;
        } else if (mMeasuredLoopGain >= 0.9999) {
            LOGE("Clipping, turn down volume slightly");
            newResult = ERROR_VOLUME_TOO_HIGH;
        } else if (mState != STATE_GOT_DATA) {
            LOGD("WARNING - Bad state. Check volume on device.");
            // setResult(ERROR_INVALID_STATE);
        } else {
            // Cleanup the signal to improve the auto-correlation.
            mAudioRecording.dcBlocker();
            mAudioRecording.square();
            mAudioRecording.lowPassFilter();

            LOGD("Please wait several seconds for auto-correlation to complete.");
            measureLatencyFromEchos(mAudioRecording.getData(),
                                    mAudioRecording.size(),
                                    getSampleRate(),
                                    &mLatencyReport);

            double latencyMillis = kMillisPerSecond * (double) mLatencyReport.latencyInFrames
                                   / getSampleRate();
            LOGD(LOOPBACK_RESULT_TAG "latency.frames         = %8.2f",
                   mLatencyReport.latencyInFrames);
            LOGD(LOOPBACK_RESULT_TAG "latency.msec           = %8.2f",
                   latencyMillis);
            LOGD(LOOPBACK_RESULT_TAG "latency.confidence     = %8.6f",
                   mLatencyReport.confidence);
            if (mLatencyReport.confidence < kMinimumConfidence) {
                LOGD("   ERROR - confidence too low!");
                newResult = ERROR_CONFIDENCE;
            }
        }
        mState = STATE_DONE;
        if (getResult() == RESULT_OK) {
            setResult(newResult);
        }
    }

    double getMeasuredLatency() override {
        return mLatencyReport.latencyInFrames;
    }

    double getMeasuredConfidence() override {
        return mLatencyReport.confidence;
    }

    void printStatus() override {
        LOGD("st = %d, echo gain = %f ", mState, mEchoGain);
    }

    int32_t getMaxLatencyFrames() {
        return 1 * getSampleRate();
    }

    result_code process(float *inputData, int inputChannelCount,
                 float *outputData, int outputChannelCount,
                 int numFrames) override {
        int channelsValid = std::min(inputChannelCount, outputChannelCount);
        float peak = 0.0f;
        int numSamples;
        mLoopCounter++;

        mTimeoutCounter -= numFrames;
        if (!isDone() && mTimeoutCounter < 0) {
            LOGW("EchoAnalyzer timed out.");
            switch (mState) {
                case STATE_MEASURING_GAIN:
                    setResult(ERROR_VOLUME_TOO_LOW);
                    break;
                case STATE_WAITING_FOR_SILENCE:
                    setResult(ERROR_NOISY);
                    break;
                default:
                    break;
            }
            mState = STATE_FAILED;
            return RESULT_OK;
        }

        echo_state nextState = mState;

        switch (mState) {
            case STATE_INITIAL_SILENCE:
                // Output silence at the beginning.
                numSamples = numFrames * outputChannelCount;
                for (int i = 0; i < numSamples; i++) {
                    outputData[i] = 0;
                }
                mDownCounter -= numFrames;
                if (mDownCounter <= 0) {
                    nextState = STATE_MEASURING_GAIN;
                    mDownCounter = getBlockFrames() * 2;
                }
                break;

            case STATE_MEASURING_GAIN:
                sendImpulses(outputData, outputChannelCount, numFrames);
                // Do we have enough data to get a reasonable measurement?
                if (numFrames > kImpulseSizeInFrames) {
                    peak = measurePeakAmplitude(inputData, inputChannelCount, numFrames);
                    // If we get several in a row then go to next state.
                    if (peak > mPulseThreshold) {
                        mDownCounter -= numFrames;
                        if (mDownCounter <= 0) {
                            mDownCounter = getBlockFrames();
                            mMeasuredLoopGain = peak;  // assumes original pulse amplitude is one
                            mSilenceThreshold = peak * 0.1; // scale silence to measured pulse
                            // Calculate gain that will give us a nice decaying echo.
                            mEchoGain = mDesiredEchoGain / mMeasuredLoopGain;
                            if (mEchoGain > kMaxEchoGain) {
                                LOGE("ERROR - loop gain too low. Increase the volume.");
                                setResult(ERROR_VOLUME_TOO_LOW);
                                nextState = STATE_FAILED;
                            } else {
                                nextState = STATE_WAITING_FOR_SILENCE;
                                mDownCounter = getMaxLatencyFrames();
                            }
                        }
                    } else {
                        mDownCounter = getBlockFrames() * 2;
                    }
                }
                break;

            case STATE_WAITING_FOR_SILENCE:
                // Output silence and wait for the echos to die down.
                numSamples = numFrames * outputChannelCount;
                for (int i = 0; i < numSamples; i++) {
                    outputData[i] = 0;
                }
                peak = measurePeakAmplitude(inputData, inputChannelCount, numFrames);
                // If we get several in a row then go to next state.
                if (peak < mSilenceThreshold) {
                    mDownCounter -= numFrames;
                    if (mDownCounter <= 0) {
                        nextState = STATE_SENDING_PULSE;
                    }
                }
                break;

            case STATE_SENDING_PULSE:
                mAudioRecording.write(inputData, inputChannelCount, numFrames);
                sendOneImpulse(outputData, outputChannelCount);
                nextState = STATE_GATHERING_ECHOS;
                break;

            case STATE_GATHERING_ECHOS:
                // Record input until the mAudioRecording is full.
                mAudioRecording.write(inputData, inputChannelCount, numFrames);
                if (hasEnoughData()) {
                    nextState = STATE_GOT_DATA;
                }

                peak = measurePeakAmplitude(inputData, inputChannelCount, numFrames);
                if (peak > mMeasuredLoopGain) {
                    mMeasuredLoopGain = peak;  // AGC might be raising gain so adjust it on the fly.
                    // Recalculate gain that will give us a nice decaying echo.
                    mEchoGain = mDesiredEchoGain / mMeasuredLoopGain;
                }

                // Echo input to output.
                for (int i = 0; i < numFrames; i++) {
                    int ic;
                    for (ic = 0; ic < channelsValid; ic++) {
                        outputData[ic] = inputData[ic] * mEchoGain;
                    }
                    for (; ic < outputChannelCount; ic++) {
                        outputData[ic] = 0;
                    }
                    inputData += inputChannelCount;
                    outputData += outputChannelCount;
                }
                break;

            case STATE_GOT_DATA:
            case STATE_DONE:
            case STATE_FAILED:
            default:
                break;
        }

        mState = nextState;
        return RESULT_OK;
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

    void sendImpulses(float *outputData, int outputChannelCount, int numFrames) {
        while (numFrames-- > 0) {
            float sample = s_Impulse[mSampleIndex++];
            if (mSampleIndex >= kImpulseSizeInFrames) {
                mSampleIndex = 0;
            }

            *outputData = sample;
            outputData += outputChannelCount;
        }
    }

    void sendOneImpulse(float *outputData, int outputChannelCount) {
        mSampleIndex = 0;
        sendImpulses(outputData, outputChannelCount, kImpulseSizeInFrames);
    }

    // @return number of frames for a typical block of processing
    int32_t getBlockFrames() {
        return getSampleRate() / 8;
    }

    enum echo_state {
        STATE_INITIAL_SILENCE,
        STATE_MEASURING_GAIN,
        STATE_WAITING_FOR_SILENCE,
        STATE_SENDING_PULSE,
        STATE_GATHERING_ECHOS,
        STATE_GOT_DATA,
        STATE_DONE,
        STATE_FAILED
    };

    const char *convertStateToText(echo_state state) {
        const char *result = "Unknown";
        switch(state) {
            case STATE_INITIAL_SILENCE:
                result = "INIT";
                break;
            case STATE_MEASURING_GAIN:
                result = "GAIN";
                break;
            case STATE_WAITING_FOR_SILENCE:
                result = "SILENCE";
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
    int32_t         mTimeoutCounter = 0; // timeout in frames so we don't stall
    int32_t         mLoopCounter = 0;
    int32_t         mSampleIndex = 0;
    float           mPulseThreshold = 0.10f;
    static constexpr float kSilenceThreshold = 0.04f;
    float           mSilenceThreshold = kSilenceThreshold;
    float           mMeasuredLoopGain = 0.0f;
    float           mDesiredEchoGain = 0.95f;
    float           mEchoGain = 1.0f;
    echo_state      mState = STATE_INITIAL_SILENCE;

    AudioRecording  mAudioRecording; // contains only the input after the gain detection burst
    LatencyReport   mLatencyReport;
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

    int32_t getGlitchCount() {
        return mGlitchCount;
    }

    double getSignalToNoiseDB() {
        static const double threshold = 1.0e-10;
        if (mMagnitude < threshold || mPeakNoise < threshold) {
            return 0.0;
        } else {
            double amplitudeRatio = mMagnitude / mPeakNoise;
            double signalToNoise = amplitudeRatio * amplitudeRatio;
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
        LOGD(LOOPBACK_RESULT_TAG "peak.amplitude     = %8f", mPeakAmplitude);
        LOGD(LOOPBACK_RESULT_TAG "sine.magnitude     = %8f", mMagnitude);
        LOGD(LOOPBACK_RESULT_TAG "peak.noise         = %8f", mPeakNoise);
        LOGD(LOOPBACK_RESULT_TAG "rms.noise          = %8f", mRootMeanSquareNoise);
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
    result_code process(float *inputData, int inputChannelCount,
                 float *outputData, int outputChannelCount,
                 int numFrames) override {
        result_code result = RESULT_OK;
        mProcessCount++;

        float peak = measurePeakAmplitude(inputData, inputChannelCount, numFrames);
        if (peak > mPeakAmplitude) {
            mPeakAmplitude = peak;
        }

        for (int i = 0; i < numFrames; i++) {
            bool sineEnabled = true;
            float sample = inputData[i * inputChannelCount];

            float sinOut = sinf(mPhase);

            switch (mState) {
                case STATE_IDLE:
                    sineEnabled = false;
                    mDownCounter--;
                    if (mDownCounter <= 0) {
                        mState = STATE_MEASURE_NOISE;
                        mDownCounter = NOISE_FRAME_COUNT;
                    }
                    break;
                case STATE_MEASURE_NOISE:
                    sineEnabled = false;
                    mPeakNoise = std::max(abs(sample), mPeakNoise);
                    mNoiseSumSquared += sample * sample;
                    mDownCounter--;
                    if (mDownCounter <= 0) {
                        mState = STATE_WAITING_FOR_SIGNAL;
                        mRootMeanSquareNoise = sqrt(mNoiseSumSquared / NOISE_FRAME_COUNT);
                        mTolerance = std::max(MIN_TOLERANCE, mPeakNoise * 2.0f);
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
                    float absDiff = fabs(diff);
                    mMaxGlitchDelta = std::max(mMaxGlitchDelta, absDiff);
                    if (absDiff > mTolerance) {
                        result = ERROR_GLITCHES;
                        addGlitch();
                    }

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
                        resetAccumulator();

                        if (mMagnitude < mThreshold) {
                            result = ERROR_GLITCHES;
                            addGlitch();
                        }
                    }
                } break;
            }

            float output = 0.0f;
            // Output sine wave so we can measure it.
            if (sineEnabled) {
                output = (sinOut * mOutputAmplitude)
                         + (mWhiteNoise.nextRandomDouble() * mNoiseAmplitude);
                // LOGD("%5d: sin(%f) = %f, %f", i, mPhase, sinOut,  mPhaseIncrement);
                // advance and wrap phase
                mPhase += mPhaseIncrement;
                if (mPhase > M_PI) {
                    mPhase -= (2.0 * M_PI);
                }
            }
            outputData[i * outputChannelCount] = output;
            mFrameCounter++;
        }
        return result;
    }

    void addGlitch() {
        mGlitchCount++;
//        LOGD("%5d: Got a glitch # %d, predicted = %f, actual = %f",
//        mFrameCounter, mGlitchCount, predicted, sample);
        mState = STATE_IMMUNE;
        mDownCounter = mSinePeriod * PERIODS_IMMUNE;
    }

    void resetAccumulator() {
        mFramesAccumulated = 0;
        mSinAccumulator = 0.0;
        mCosAccumulator = 0.0;
    }

    void reset() override {
        LoopbackProcessor::reset();
        mGlitchCount = 0;
        mState = STATE_IDLE;
        mDownCounter = IDLE_FRAME_COUNT;
        mPhaseIncrement = 2.0 * M_PI / mSinePeriod;
        resetAccumulator();
        mProcessCount = 0;
        mPeakNoise = 0.0f;
        mNoiseSumSquared = 0.0;
        mRootMeanSquareNoise = 0.0;
        mPhase = 0.0f;
        mMaxGlitchDelta = 0.0;
    }

private:

    enum sine_state_t {
        STATE_IDLE,
        STATE_MEASURE_NOISE,
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

    static constexpr float MIN_TOLERANCE = 0.01;

    int     mSinePeriod = 79;
    double  mPhaseIncrement = 0.0;
    double  mPhase = 0.0;
    double  mPhaseOffset = 0.0;
    double  mPreviousPhaseOffset = 0.0;
    double  mMagnitude = 0.0;
    double  mThreshold = 0.005;
    double  mTolerance = MIN_TOLERANCE;
    int32_t mFramesAccumulated = 0;
    int32_t mProcessCount = 0;
    double  mSinAccumulator = 0.0;
    double  mCosAccumulator = 0.0;
    float   mMaxGlitchDelta = 0.0f;
    int32_t mGlitchCount = 0;
    double  mPeakAmplitude = 0.0;
    int     mDownCounter = IDLE_FRAME_COUNT;
    int32_t mFrameCounter = 0;
    float   mOutputAmplitude = 0.75;

    // measure background noise
    float   mPeakNoise = 0.0f;
    double  mNoiseSumSquared = 0.0;
    double  mRootMeanSquareNoise = 0.0;

    PseudoRandom  mWhiteNoise;
    float   mNoiseAmplitude = 0.00; // Used to experiment with warbling caused by DRC.

    sine_state_t  mState = STATE_IDLE;
};

#undef LOOPBACK_RESULT_TAG

#endif // OBOETESTER_LATENCY_ANALYSER_H
