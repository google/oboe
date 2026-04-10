#ifndef NATIVEOBOE_EXACTBLIPGENERATOR_H
#define NATIVEOBOE_EXACTBLIPGENERATOR_H

#include <atomic>
#include <math.h>
#include "flowgraph/FlowGraphNode.h"

class BlipGenerator : public oboe::flowgraph::FlowGraphNode {
public:
    BlipGenerator();
    virtual ~BlipGenerator() = default;

    void setSampleRate(int sampleRate);
    int32_t onProcess(int numFrames) override;
    void trigger();
    void reset() override;

    oboe::flowgraph::FlowGraphPortFloatOutput output;

private:
    static const int WAVETABLE_LENGTH = 2049;
    static const int NUM_WAVETABLE_SAMPLES = 2048; // LENGTH - 1

    static const int NUM_PULSE_FRAMES = (int) (48000 * (1.0 / 16.0));

    float mWaveTable[WAVETABLE_LENGTH];
    float mSrcPhase;
    float mPhaseIncr;
    int mNumPendingPulseFrames;

    std::atomic<int> mRequestCount;
    std::atomic<int> mAcknowledgeCount;
};

#endif // NATIVEOBOE_EXACTBLIPGENERATOR_H