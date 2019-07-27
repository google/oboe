/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef OBOETESTER_RANDOM_PULSE_GENERATOR_H
#define OBOETESTER_RANDOM_PULSE_GENERATOR_H

#include <math.h>
#include <stdlib.h>

/**
 * Encode ones and zeros using Manchester Code per IEEE 802.3
 * There is a transition in the middle of every bit.
 * Zero is high then low.
 * One is low then high.
 *
 * This avoid having long DC sections that would droop when
 * passed though analog circuits with AC coupling.
 * The edges are shaped using a half cosine to reduce ringing.
 */
class RandomPulseGenerator {
public:
    RandomPulseGenerator(int framesPerPulse)
    : mFramesPerPulse(framesPerPulse)
    , mCursor(framesPerPulse) {
        int rampSize = framesPerPulse / 4;
        mZeroAfterZero = std::make_unique<float[]>(framesPerPulse);
        mZeroAfterOne = std::make_unique<float[]>(framesPerPulse);

        int i = 0;
        for (int j = 0; j < rampSize; j++) {
            float phase = (j + 1) * M_PI / rampSize;
            float sample = -cos(phase);
            mZeroAfterZero[i] = sample;
            mZeroAfterOne[i] = 1.0f;
            i++;
        }
        for (int j = 0; j < rampSize; j++) {
            mZeroAfterZero[i] = 1.0f;
            mZeroAfterOne[i] = 1.0f;
            i++;
        }
        for (int j = 0; j < rampSize; j++) {
            float phase = (j + 1) * M_PI / rampSize;
            float sample = cos(phase);
            mZeroAfterZero[i] = sample;
            mZeroAfterOne[i] = sample;
            i++;
        }
        for (int j = 0; j < rampSize; j++) {
            mZeroAfterZero[i] = -1.0f;
            mZeroAfterOne[i] = -1.0f;
            i++;
        }
    }

    float next() {
        // Are we ready for a new bit?
        if (++mCursor >= mFramesPerPulse) {
            mCurrentBit = (rand() & 1) == 1;  // new random bit
            // Do we need to use the rounded edge?
            mCurrentSamples = (mCurrentBit ^ mPreviousBit)
                    ? mZeroAfterOne.get()
                    : mZeroAfterZero.get();
            mCursor = 0;
            mPreviousBit = mCurrentBit;
            printf("bit = %d ------\n", mCurrentBit ? 1 : 0);
        }
        float output = mCurrentSamples[mCursor];
        if (mCurrentBit) output = -output;
        return output;
    }

private:
    const int mFramesPerPulse;
    int mCursor = 0;
    bool mPreviousBit = false;
    bool mCurrentBit = false;
    float *mCurrentSamples = nullptr;
    std::unique_ptr<float[]> mZeroAfterZero;
    std::unique_ptr<float[]> mZeroAfterOne;
};

#endif //OBOETESTER_RANDOM_PULSE_GENERATOR_H
