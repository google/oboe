/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include "FrequencyAnalyzer.h"
#include <math.h>
#include "fft.h"

FrequencyAnalyzer::FrequencyAnalyzer() : LoopbackProcessor() {
    mWindow.resize(WINDOW_SIZE);
    mIncoherentPower = 0.0;
    mWindowSum = 0.0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        mWindow[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (WINDOW_SIZE - 1)));
        mIncoherentPower += mWindow[i] * mWindow[i];
        mWindowSum += mWindow[i];
    }
    mInputBuffer.resize(WINDOW_SIZE);
    mAverageBuffer.resize(WINDOW_SIZE / 2);

    mInputBufferIndex = 0;
    mFramesAccumulated = 0;
    mOutputPhase = 0.0f;

    std::lock_guard<std::mutex> lock(mFftBufferLock);
    mFftMagnitudeBuffer.clear();
}

void FrequencyAnalyzer::reset() {
    LoopbackProcessor::reset();
    mInputBufferIndex = 0;
    mOutputPhase = 0.0f;

    mAverageBuffer.clear();
    mFramesAccumulated = 0;

    std::lock_guard<std::mutex> lock(mFftBufferLock);
    mFftMagnitudeBuffer.clear();
}

void FrequencyAnalyzer::prepareToTest() {
    LoopbackProcessor::prepareToTest();
    mPhaseIncrement = 2.0 * M_PI * SINE_WAVE_FREQUENCY / getSampleRate();
    mMeasurementWindowFrames = MEASUREMENT_TIME_SEC * getSampleRate();
}

LoopbackProcessor::result_code FrequencyAnalyzer::processInputFrame(const float* frameData,
                                                                    int channelCount) {
    float sample = frameData[getInputChannel()];
    mInputBuffer[mInputBufferIndex++] = sample;
    if (mInputBufferIndex >= WINDOW_SIZE) {
        // Perform FFT
        std::lock_guard<std::mutex> lock(mFftBufferLock);
        CVector fftInput(WINDOW_SIZE);
        for (int i = 0; i < WINDOW_SIZE; i++) {
            double windowed = mInputBuffer[i] * mWindow[i];
            fftInput[i] = Complex(windowed, 0);
        }
        fft(fftInput);

        // Accumulate magnitude
        std::vector<double> currentMagnitudes(WINDOW_SIZE / 2);
        for (int i = 0; i < WINDOW_SIZE / 2; i++) {
            double mag;
            if (mSignalType == 1) { // Sine
                mag = 2.0 * std::abs(fftInput[i]) / mWindowSum;
            } else {
                mag = 4.0 * std::abs(fftInput[i]) / std::sqrt(mIncoherentPower);
            }
            if (mag < 1e-9) mag = 1e-9; // to prevent log(0)
            currentMagnitudes[i] = mag;
        }
        mAverageBuffer.accumulate(currentMagnitudes.data(),
                                  currentMagnitudes.size());
        mInputBufferIndex = 0;
    }

    mFramesAccumulated++;
    if (mFramesAccumulated >= mMeasurementWindowFrames) {
        // End of measurement window! Compute the average and save it to mFftMagnitudeBuffer
        std::lock_guard<std::mutex> lock(mFftBufferLock);
        if (mFftMagnitudeBuffer.empty()) {
            // First measurement window
            mFftMagnitudeBuffer.resize(WINDOW_SIZE / 2);
        }
        for (int i = 0; i < WINDOW_SIZE / 2; i++) {
            double avgMag = mAverageBuffer.getAverageAt(i);
            double dbfs = 20.0 * std::log10(avgMag);
            mFftMagnitudeBuffer[i] = static_cast<float>(dbfs);
        }
        mAverageBuffer.clear();
        mFramesAccumulated = 0;
    }

    return RESULT_OK;
}

void FrequencyAnalyzer::setSignalType(int signalType) {
    mSignalType = signalType;
}

int FrequencyAnalyzer::getWindowSize() {
    return WINDOW_SIZE;
}

LoopbackProcessor::result_code FrequencyAnalyzer::processOutputFrame(float* frameData,
                                                                     int channelCount) {
    float output = 0.0f;
    if (mSignalType == 0) { // White noise
        output = mWhiteNoise.nextRandomDouble() * mAmplitude;
    } else if (mSignalType == 1) { // Sine
        output = sin(mOutputPhase) * mAmplitude;
        mOutputPhase += mPhaseIncrement;
        if (mOutputPhase >= M_PI * 2) mOutputPhase -= M_PI * 2;
    } else if (mSignalType == 2) { // Silence
        output = 0.0f;
    }
    if (channelCount == 2) {
        float leftGain = (mBalance <= 0.5f) ? 1.0f : 2.0f * (1.0f - mBalance);
        float rightGain = (mBalance >= 0.5f) ? 1.0f : 2.0f * mBalance;
        frameData[0] = output * leftGain;
        frameData[1] = output * rightGain;
    } else {
        for (int i = 0; i < channelCount; i++) {
            frameData[i] = output;
        }
    }
    return RESULT_OK;
}

std::string FrequencyAnalyzer::analyze() {
    return "FrequencyAnalyzer: Analysis the frequency magnitude";
}

bool FrequencyAnalyzer::isDone() {
    return false;
}

int FrequencyAnalyzer::getFftMagnitude(float* buffer, int length) {
    std::lock_guard<std::mutex> lock(mFftBufferLock);
    if (mFftMagnitudeBuffer.empty()) {
        return 0;
    }
    int numToCopy = std::min(length, (int) (WINDOW_SIZE / 2));
    for (int i = 0; i < numToCopy; i++) {
        buffer[i] = mFftMagnitudeBuffer[i];
    }
    return numToCopy;
}

int FrequencyAnalyzer::getFftFrequencies(float* buffer, int length) {
    int numToCopy = std::min(length, (int) (WINDOW_SIZE / 2));
    double sampleRate = getSampleRate();
    for (int i = 0; i < numToCopy; i++) {
        buffer[i] = (float) (i * sampleRate / WINDOW_SIZE);
    }
    return numToCopy;
}
