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

#ifndef OBOETESTER_GLITCHANALYZER_H
#define OBOETESTER_GLITCHANALYZER_H

#include <cctype>

#include "PseudoRandom.h"
#include "LatencyAnalyzer.h"
#include "InfiniteRecording.h"

/**
 * Output a steady sinewave and analyze the return signal.
 *
 * Use a cosine transform to measure the predicted magnitude and relative phase of the
 * looped back sine wave. Then generate a predicted signal and compare with the actual signal.
 */
class GlitchAnalyzer : public LoopbackProcessor {
public:

    GlitchAnalyzer()
            : LoopbackProcessor()
            , mInfiniteRecording(64 * 1024) {}

    int32_t getState() {
        return mState;
    }

    float getPeakAmplitude() {
        return mPeakFollower.getLevel();
    }

    float getTolerance() {
        return mTolerance;
    }

    void setTolerance(float tolerance) {
        mTolerance = tolerance;
        mScaledTolerance = mMagnitude * mTolerance;
    }

    void setMagnitude(double magnitude) {
        mMagnitude = magnitude;
        mScaledTolerance = mMagnitude * mTolerance;
    }

    int32_t getGlitchCount() {
        return mGlitchCount;
    }

    int32_t getStateFrameCount(int state) {
        return mStateFrameCounters[state];
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
     * @param frameData contains microphone data with sine signal feedback
     * @param channelCount
     */
    result_code processInputFrame(float *frameData, int channelCount) override {
        result_code result = RESULT_OK;

        float sample = frameData[0];
        float peak = mPeakFollower.process(sample);
        mInfiniteRecording.write(sample);

        // Force a periodic glitch!
        if (mForceGlitchDuration > 0) {
            if (mForceGlitchCounter == 0) {
                LOGE("%s: force a glitch!!", __func__);
                mForceGlitchCounter = getSampleRate();
            } else if (mForceGlitchCounter <= mForceGlitchDuration) {
                sample += (sample > 0.0) ? -0.5f : 0.5f;
            }
            --mForceGlitchCounter;
        }

        mStateFrameCounters[mState]++; // count how many frames we are in each state

        switch (mState) {
            case STATE_IDLE:
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mState = STATE_IMMUNE;
                    mDownCounter = IMMUNE_FRAME_COUNT;
                    mInputPhase = 0.0; // prevent spike at start
                    mOutputPhase = 0.0;
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
                mSinAccumulator += sample * sinf(mInputPhase);
                mCosAccumulator += sample * cosf(mInputPhase);
                mFramesAccumulated++;
                // Must be a multiple of the period or the calculation will not be accurate.
                if (mFramesAccumulated == mSinePeriod * PERIODS_NEEDED_FOR_LOCK) {
                    double phaseOffset = 0.0;
                    setMagnitude(calculateMagnitude(&phaseOffset));
//                    LOGD("%s() mag = %f, offset = %f, prev = %f",
//                            __func__, mMagnitude, mPhaseOffset, mPreviousPhaseOffset);
                    if (mMagnitude > mThreshold) {
                        if (abs(phaseOffset) < kMaxPhaseError) {
                            mState = STATE_LOCKED;
//                            LOGD("%5d: switch to STATE_LOCKED", mFrameCounter);
                        }
                        // Adjust mInputPhase to match measured phase
                        mInputPhase += phaseOffset;
                    }
                    resetAccumulator();
                }
                incrementInputPhase();
                break;

            case STATE_LOCKED: {
                // Predict next sine value
                float predicted = sinf(mInputPhase) * mMagnitude;
                float diff = predicted - sample;
                float absDiff = fabs(diff);
                mMaxGlitchDelta = std::max(mMaxGlitchDelta, absDiff);
                if (absDiff > mScaledTolerance) {
                    result = ERROR_GLITCHES;
                    onGlitchStart();
//                    LOGI("diff glitch detected, absDiff = %g", absDiff);
                } else {
                    mSumSquareSignal += predicted * predicted;
                    mSumSquareNoise += diff * diff;
                    // Track incoming signal and slowly adjust magnitude to account
                    // for drift in the DRC or AGC.
                    mSinAccumulator += sample * sinf(mInputPhase);
                    mCosAccumulator += sample * cosf(mInputPhase);
                    mFramesAccumulated++;
                    // Must be a multiple of the period or the calculation will not be accurate.
                    if (mFramesAccumulated == mSinePeriod) {
                        const double coefficient = 0.1;
                        double phaseOffset = 0.0;
                        double magnitude = calculateMagnitude(&phaseOffset);
                        // One pole averaging filter.
                        setMagnitude((mMagnitude * (1.0 - coefficient)) + (magnitude * coefficient));

                        mMeanSquareNoise = mSumSquareNoise * mInverseSinePeriod;
                        mMeanSquareSignal = mSumSquareSignal * mInverseSinePeriod;
                        resetAccumulator();

                        if (abs(phaseOffset) > kMaxPhaseError) {
                            result = ERROR_GLITCHES;
                            onGlitchStart();
                            LOGD("phase glitch detected, phaseOffset = %g", phaseOffset);
                        } else if (mMagnitude < mThreshold) {
                            result = ERROR_GLITCHES;
                            onGlitchStart();
                            LOGD("magnitude glitch detected, mMagnitude = %g", mMagnitude);
                        }
                    }
                }
                incrementInputPhase();
            } break;

            case STATE_GLITCHING: {
                // Predict next sine value
                mGlitchLength++;
                float predicted = sinf(mInputPhase) * mMagnitude;
                float diff = predicted - sample;
                float absDiff = fabs(diff);
                mMaxGlitchDelta = std::max(mMaxGlitchDelta, absDiff);
                if (absDiff < mScaledTolerance) { // close enough?
                    // If we get a full sine period of non-glitch samples in a row then consider the glitch over.
                    // We don't want to just consider a zero crossing the end of a glitch.
                    if (mNonGlitchCount++ > mSinePeriod) {
                        onGlitchEnd();
                    }
                } else {
                    mNonGlitchCount = 0;
                    if (mGlitchLength > (4 * mSinePeriod)) {
                        relock();
                    }
                }
                incrementInputPhase();
            } break;

            case NUM_STATES: // not a real state
                break;
        }

        mFrameCounter++;

        return result;
    }

    // advance and wrap phase
    void incrementInputPhase() {
        mInputPhase += mPhaseIncrement;
        if (mInputPhase > M_PI) {
            mInputPhase -= (2.0 * M_PI);
        }
    }

    // advance and wrap phase
    void incrementOutputPhase() {
        mOutputPhase += mPhaseIncrement;
        if (mOutputPhase > M_PI) {
            mOutputPhase -= (2.0 * M_PI);
        }
    }

    /**
     * @param frameData upon return, contains the reference sine wave
     * @param channelCount
     */
    result_code processOutputFrame(float *frameData, int channelCount) override {
        float output = 0.0f;
        // Output sine wave so we can measure it.
        if (mState != STATE_IDLE) {
            float sinOut = sinf(mOutputPhase);
            incrementOutputPhase();
            output = (sinOut * mOutputAmplitude)
                     + (mWhiteNoise.nextRandomDouble() * kNoiseAmplitude);
            // LOGD("%5d: sin(%f) = %f, %f", i, mPhase, sinOut,  mPhaseIncrement);
        }
        frameData[0] = output;
        for (int i = 1; i < channelCount; i++) {
            frameData[i] = 0.0f;
        }
        return RESULT_OK;
    }

    void onGlitchStart() {
        mGlitchCount++;
//        LOGD("%5d: STARTED a glitch # %d", mFrameCounter, mGlitchCount);
        mState = STATE_GLITCHING;
        mGlitchLength = 1;
        mNonGlitchCount = 0;
        mLastGlitchPosition = mInfiniteRecording.getTotalWritten();
    }

    void onGlitchEnd() {
//        LOGD("%5d: ENDED a glitch # %d, length = %d", mFrameCounter, mGlitchCount, mGlitchLength);
        mState = STATE_LOCKED;
        resetAccumulator();
    }

    // reset the sine wave detector
    void resetAccumulator() {
        mFramesAccumulated = 0;
        mSinAccumulator = 0.0;
        mCosAccumulator = 0.0;
        mSumSquareSignal = 0.0;
        mSumSquareNoise = 0.0;
    }

    void relock() {
//        LOGD("relock: %d because of a very long %d glitch", mFrameCounter, mGlitchLength);
        mState = STATE_WAITING_FOR_LOCK;
        resetAccumulator();
    }

    void reset() override {
        LoopbackProcessor::reset();
        mState = STATE_IDLE;
        mDownCounter = IDLE_FRAME_COUNT;
        resetAccumulator();
    }

    void onStartTest() override {
        LoopbackProcessor::onStartTest();
        mSinePeriod = getSampleRate() / kTargetGlitchFrequency;
        mOutputPhase = 0.0f;
        mInverseSinePeriod = 1.0 / mSinePeriod;
        mPhaseIncrement = 2.0 * M_PI * mInverseSinePeriod;
        mGlitchCount = 0;
        mMaxGlitchDelta = 0.0;
        for (int i = 0; i < NUM_STATES; i++) {
            mStateFrameCounters[i] = 0;
        }
    }


    int32_t getLastGlitch(float *buffer, int32_t length) {
        return mInfiniteRecording.readFrom(buffer, mLastGlitchPosition - 32, length);
    }

private:

    // These must match the values in GlitchActivity.java
    enum sine_state_t {
        STATE_IDLE,               // beginning
        STATE_IMMUNE,             // ignoring input, waiting fo HW to settle
        STATE_WAITING_FOR_SIGNAL, // looking for a loud signal
        STATE_WAITING_FOR_LOCK,   // trying to lock onto the phase of the sine
        STATE_LOCKED,             // locked on the sine wave, looking for glitches
        STATE_GLITCHING,           // locked on the sine wave but glitching
        NUM_STATES
    };

    enum constants {
        // Arbitrary durations, assuming 48000 Hz
                IDLE_FRAME_COUNT = 48 * 100,
        IMMUNE_FRAME_COUNT = 48 * 100,
        PERIODS_NEEDED_FOR_LOCK = 8,
        MIN_SNRATIO_DB = 65
    };

    static constexpr float kNoiseAmplitude = 0.00; // Used to experiment with warbling caused by DRC.
    static constexpr int kTargetGlitchFrequency = 607;
    static constexpr double kMaxPhaseError = M_PI * 0.05;

    float   mTolerance = 0.10; // scaled from 0.0 to 1.0
    double  mThreshold = 0.005;
    int     mSinePeriod = 1; // this will be set before use
    double  mInverseSinePeriod = 1.0;

    int32_t mStateFrameCounters[NUM_STATES];

    double  mPhaseIncrement = 0.0;
    double  mInputPhase = 0.0;
    double  mOutputPhase = 0.0;
    double  mMagnitude = 0.0;
    int32_t mFramesAccumulated = 0;
    double  mSinAccumulator = 0.0;
    double  mCosAccumulator = 0.0;
    float   mMaxGlitchDelta = 0.0f;
    int32_t mGlitchCount = 0;
    int32_t mNonGlitchCount = 0;
    int32_t mGlitchLength = 0;
    float   mScaledTolerance = 0.0;
    int     mDownCounter = IDLE_FRAME_COUNT;
    int32_t mFrameCounter = 0;
    float   mOutputAmplitude = 0.75;

    int32_t mForceGlitchDuration = 0; // if > 0 then force a glitch for debugging
    int32_t mForceGlitchCounter = 4 * 48000; // count down and trigger at zero

    // measure background noise continuously as a deviation from the expected signal
    double  mSumSquareSignal = 0.0;
    double  mSumSquareNoise = 0.0;
    double  mMeanSquareSignal = 0.0;
    double  mMeanSquareNoise = 0.0;

    PeakDetector  mPeakFollower;

    PseudoRandom  mWhiteNoise;

    sine_state_t  mState = STATE_IDLE;

    InfiniteRecording<float> mInfiniteRecording;
    int64_t       mLastGlitchPosition;
};


#endif //OBOETESTER_GLITCHANALYZER_H
