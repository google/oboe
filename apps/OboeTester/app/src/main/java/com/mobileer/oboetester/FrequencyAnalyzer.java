/*
 * Copyright 2026 The Android Open Source Project
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

package com.mobileer.oboetester;

import java.util.List;

public class FrequencyAnalyzer {

    public static class AnalysisResult {

        public float[] thresholdFrequencies;
        public float[] alignedTopThresholdsDbfs;
        public float[] alignedBottomThresholdsDbfs;
        public boolean testPassed;
        public float[] bandEnergyPercentages;
        public float averageMagnitudeBand1;
    }

    public AnalysisResult analyze(float[] waveformBuffer, int numSamples, float[] frequencies,
            int numFreqs, FrequencyBandSpec spec, float passThreshold, boolean alignToBand1) {
        if (spec == null || spec.getFrequencyAnchors() == null || spec.getBands() == null) {
            return null;
        }
        int[] anchors = spec.getFrequencyAnchors();
        int numPoints = anchors.length;
        if (numPoints <= 0) {
            return null;
        }

        AnalysisResult result = new AnalysisResult();

        int numBands = numPoints - 1;
        int[] pointsInBounds = new int[numBands];
        int[] totalPoints = new int[numBands];
        int limit = Math.min(numSamples, numFreqs);

        // Calculate averageMagnitudeBand1 for threshold alignment
        float averageMagnitudeBand1 = 0.0f;
        if (alignToBand1 && numFreqs > 0 && numPoints >= 3) {
            float band1StartFreq = anchors[1];
            float band1StopFreq = anchors[2];
            float sumMag = 0.0f;
            int countMag = 0;
            for (int i = 0; i < limit; i++) {
                if (frequencies[i] >= band1StartFreq && frequencies[i] <= band1StopFreq) {
                    sumMag += waveformBuffer[i];
                    countMag++;
                }
            }
            if (countMag > 0) {
                averageMagnitudeBand1 = sumMag / countMag;
            }
        }

        for (int i = 0; i < limit; i++) {
            float freq = frequencies[i];
            float mag = waveformBuffer[i];

            for (int b = 0; b < numBands; b++) {
                if (freq >= anchors[b] && freq <= anchors[b + 1]) {
                    totalPoints[b]++;

                    float alignedTop = spec.getTopThresholdAt(freq) + averageMagnitudeBand1;
                    float alignedBottom = spec.getBottomThresholdAt(freq) + averageMagnitudeBand1;

                    if (mag >= alignedBottom && mag <= alignedTop) {
                        pointsInBounds[b]++;
                    }
                    break;
                }
            }
        }

        result.bandEnergyPercentages = new float[numBands];
        boolean allPass = true;
        for (int b = 0; b < numBands; b++) {
            if (totalPoints[b] > 0) {
                result.bandEnergyPercentages[b] =
                        (pointsInBounds[b] / (float) totalPoints[b]) * 100.0f;
                if (result.bandEnergyPercentages[b] < passThreshold) {
                    allPass = false;
                }
            } else {
                result.bandEnergyPercentages[b] = 0.0f;
                allPass = false;
            }
        }

        boolean band1CheckPassed = true;
        FrequencyPreset.Band1CheckType checkType = spec.getBand1CheckType();
        float band1Threshold = spec.getBand1Threshold();
        if (checkType == FrequencyPreset.Band1CheckType.GREATER_THAN) {
            band1CheckPassed = averageMagnitudeBand1 > band1Threshold;
        } else if (checkType == FrequencyPreset.Band1CheckType.LESS_THAN) {
            band1CheckPassed = averageMagnitudeBand1 < band1Threshold;
        }

        result.testPassed = allPass && band1CheckPassed;

        result.thresholdFrequencies = new float[numBands * 2];
        result.alignedTopThresholdsDbfs = new float[numBands * 2];
        result.alignedBottomThresholdsDbfs = new float[numBands * 2];

        List<FrequencyBandSpec.BandThreshold> bands = spec.getBands();
        for (int b = 0; b < numBands; b++) {
            result.thresholdFrequencies[b * 2] = anchors[b];
            result.thresholdFrequencies[b * 2 + 1] = anchors[b + 1];
            result.alignedTopThresholdsDbfs[b * 2] = bands.get(b).startTop + averageMagnitudeBand1;
            result.alignedTopThresholdsDbfs[b * 2 + 1] =
                    bands.get(b).stopTop + averageMagnitudeBand1;
            result.alignedBottomThresholdsDbfs[b * 2] =
                    bands.get(b).startBottom + averageMagnitudeBand1;
            result.alignedBottomThresholdsDbfs[b * 2 + 1] =
                    bands.get(b).stopBottom + averageMagnitudeBand1;
        }
        result.averageMagnitudeBand1 = averageMagnitudeBand1;

        return result;
    }
}
