/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ANALYZER_DATA_PATH_ANALYZER_H
#define ANALYZER_DATA_PATH_ANALYZER_H

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>

#include "BaseSineAnalyzer.h"
#include "InfiniteRecording.h"
#include "LatencyAnalyzer.h"

/**
 * Output a steady sine wave and analyze the return signal.
 *
 * Use a cosine transform to measure the predicted magnitude and relative phase of the
 * looped back sine wave.
 */
class DataPathAnalyzer : public BaseSineAnalyzer {
public:

    DataPathAnalyzer() : BaseSineAnalyzer() {}

    /**
     * @param frameData contains microphone data with sine signal feedback
     * @param channelCount
     */
    result_code processInputFrame(float *frameData, int /* channelCount */) override {
        result_code result = RESULT_OK;

        float sample = frameData[mInputChannel];
        mInfiniteRecording.write(sample);

        double phaseOffset = 0.0;
        if (transformSample(sample, mOutputPhase, &phaseOffset)) {
            resetAccumulator();
        }
        // TODO measure phase jitter to detect noise

        return result;
    }

    std::string analyze() override {
        std::stringstream report;
        report << "DataPathAnalyzer ------------------\n";
        report << LOOPBACK_RESULT_TAG "sine.magnitude     = " << std::setw(8)
               << mMagnitude << "\n";
        report << LOOPBACK_RESULT_TAG "frames.accumulated = " << std::setw(8)
               << mFramesAccumulated << "\n";
        report << LOOPBACK_RESULT_TAG "sine.period        = " << std::setw(8)
               << mSinePeriod << "\n";
        return report.str();
    }

};
#endif // ANALYZER_DATA_PATH_ANALYZER_H
