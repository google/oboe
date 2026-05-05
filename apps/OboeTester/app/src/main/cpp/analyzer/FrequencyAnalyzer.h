#ifndef OBOETESTER_FREQUENCYANALYZER_H
#define OBOETESTER_FREQUENCYANALYZER_H

#include <vector>
#include <mutex>
#include <string>
#include "oboe/Oboe.h"
#include "LatencyAnalyzer.h"
#include "PseudoRandom.h"
#include "AverageBuffer.h"

/**
 * Analyze frequency response by playing a stimulus and measuring the input.
 */
class FrequencyAnalyzer : public LoopbackProcessor {
public:
    FrequencyAnalyzer();
    virtual ~FrequencyAnalyzer() = default;

    void reset() override;
    void prepareToTest() override;

    result_code processInputFrame(const float *frameData, int channelCount) override;
    void setSignalType(int signalType);
    int getWindowSize();
    result_code processOutputFrame(float *frameData, int channelCount) override;

    std::string analyze() override;
    bool isDone() override;

    int getFftMagnitude(float *buffer, int length);
    int getFftFrequencies(float *buffer, int length);

private:
    const static int   MEASUREMENT_TIME = 2; // Second
    const static int   SINE_WAVE_FREQUENCY = 1000; // Hz
    const static int   WINDOW_SIZE = 4096;

    PseudoRandom  mWhiteNoise;
    float         mAmplitude = 0.5f;
    int           mSignalType = 0;
    double        mOutputPhase = 0.0;
    double        mPhaseIncrement = 0.0;

    std::vector<float> mInputBuffer;
    std::vector<float> mFftMagnitudeBuffer;
    std::mutex         mFftBufferLock;
    int                mInputBufferIndex = 0;
    std::vector<double> mWindow;
    double             mIncoherentPower = 0.0;
    AverageBuffer      mAverageBuffer;
    int                mMeasurementWindowFrames = 0;
    int                mFramesAccumulated = 0;
};

#endif //OBOETESTER_FREQUENCYANALYZER_H
