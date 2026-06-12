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

#ifndef OBOETESTER_FREQUENCYANALYZER_H
#define OBOETESTER_FREQUENCYANALYZER_H

#include <vector>
#include <mutex>
#include <string>
#include <thread>
#include <condition_variable>
#include "oboe/Oboe.h"
#include "LatencyAnalyzer.h"
#include "PseudoRandom.h"
#include "AverageBuffer.h"
#include "fft.h"

/**
 * Analyze frequency response by playing a stimulus and measuring the input.
 */
class FrequencyAnalyzer : public LoopbackProcessor {
public:
    FrequencyAnalyzer();
    virtual ~FrequencyAnalyzer();

    void reset() override;
    void prepareToTest() override;
    void stop();

    result_code processInputFrame(const float* frameData,
                                  int channelCount) override;
    void setSignalType(int signalType);
    void setBalance(float balance) { mBalance = balance; }
    int getWindowSize();
    result_code processOutputFrame(float* frameData, int channelCount) override;

    std::string analyze() override;
    bool isDone() override;

    int getFftMagnitude(float* buffer, int length);
    int getFftFrequencies(float* buffer, int length);

private:
    const static int MEASUREMENT_TIME_SEC = 2;
    const static int SINE_WAVE_FREQUENCY = 1000; // Hz
    const static int WINDOW_SIZE = 4096;

    const static int FFT_THREAD_WAITTING = 0;
    const static int FFT_THREAD_READY = 1;
    const static int FFT_THREAD_END = 2;

    PseudoRandom mWhiteNoise;
    float mAmplitude = 0.8f;
    float mBalance = 0.5f;
    int mSignalType = 0;
    double mOutputPhase = 0.0;
    double mPhaseIncrement = 0.0;

    std::vector<float> mInputBuffer;
    std::vector<float> mFftMagnitudeBuffer;
    std::mutex mFftBufferLock;
    int mInputBufferIndex = 0;
    std::vector<double> mWindow;
    double mIncoherentPower = 0.0;
    double mWindowSum = 0.0;
    AverageBuffer mAverageBuffer;
    int mMeasurementWindowFrames = 0;
    int mFramesAccumulated = 0;
    std::mutex mFftThreadLock;
    std::unique_ptr<std::thread> mFftThread;
    std::condition_variable mFftThreadCond;
    int mFftThreadFlag;
    CVector mFftThreadInput;

    void fftThreadFunction();
    void joinFftThread();
};

#endif //OBOETESTER_FREQUENCYANALYZER_H
