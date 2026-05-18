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

public class FrequencyBandSpec {

    private int[] mFrequencyAnchors;
    private List<BandThreshold> mBands;
    private FrequencyPreset.Band1CheckType mBand1CheckType = FrequencyPreset.Band1CheckType.NONE;
    private float mBand1Threshold = 0.0f;

    public static class BandThreshold {

        public float startTop;
        public float stopTop;
        public float startBottom;
        public float stopBottom;

        public BandThreshold(float startTop, float stopTop, float startBottom, float stopBottom) {
            this.startTop = startTop;
            this.stopTop = stopTop;
            this.startBottom = startBottom;
            this.stopBottom = stopBottom;
        }
    }

    public FrequencyBandSpec(int[] frequencyAnchors, List<BandThreshold> bands,
            FrequencyPreset.Band1CheckType band1CheckType, float band1Threshold) {
        this.mFrequencyAnchors = frequencyAnchors;
        this.mBands = bands;
        this.mBand1CheckType = band1CheckType;
        this.mBand1Threshold = band1Threshold;
    }

    public FrequencyBandSpec(int[] frequencyAnchors, List<BandThreshold> bands) {
        this(frequencyAnchors, bands, FrequencyPreset.Band1CheckType.NONE, 0.0f);
    }

    public int[] getFrequencyAnchors() {
        return mFrequencyAnchors;
    }

    public List<BandThreshold> getBands() {
        return mBands;
    }

    public FrequencyPreset.Band1CheckType getBand1CheckType() {
        return mBand1CheckType;
    }

    public float getBand1Threshold() {
        return mBand1Threshold;
    }


    public float getTopThresholdAt(float freq) {
        return getThresholdAt(freq, true);
    }

    public float getBottomThresholdAt(float freq) {
        return getThresholdAt(freq, false);
    }

    private float getThresholdAt(float freq, boolean isTop) {
        if (mFrequencyAnchors == null || mBands == null || mFrequencyAnchors.length < 2
                || mBands.isEmpty()) {
            return 0.0f;
        }
        // Frequency below first anchor
        if (freq <= mFrequencyAnchors[0]) {
            BandThreshold first = mBands.get(0);
            return isTop ? first.startTop : first.startBottom;
        }
        // Frequency above last anchor
        if (freq >= mFrequencyAnchors[mFrequencyAnchors.length - 1]) {
            BandThreshold last = mBands.get(mBands.size() - 1);
            return isTop ? last.stopTop : last.stopBottom;
        }
        // Find the band
        for (int i = 0; i < mFrequencyAnchors.length - 1; i++) {
            if (freq >= mFrequencyAnchors[i] && freq <= mFrequencyAnchors[i + 1]) {
                float f0 = mFrequencyAnchors[i];
                float f1 = mFrequencyAnchors[i + 1];
                BandThreshold band = mBands.get(i);
                float start = isTop ? band.startTop : band.startBottom;
                float stop = isTop ? band.stopTop : band.stopBottom;
                // Linear interpolation
                return start + (stop - start) * (freq - f0) / (f1 - f0);
            }
        }
        return 0.0f;
    }
}
