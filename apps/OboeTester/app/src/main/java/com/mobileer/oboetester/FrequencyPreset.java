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

public class FrequencyPreset {

    public enum Band1CheckType {
        NONE,
        GREATER_THAN,
        LESS_THAN
    }

    final public String name;
    final public int sourceResId;
    final public int inputPreset;
    final public int[] anchors;
    final public List<FrequencyBandSpec.BandThreshold> bands;
    final public float balance;
    final public float passThreshold;
    final public int preferredInput;
    final public int preferredOutput;
    final public Band1CheckType band1CheckType;
    final public float band1Threshold;

    public FrequencyPreset(String name, int sourceResId, int inputPreset, int[] anchors,
            List<FrequencyBandSpec.BandThreshold> bands, float passThreshold, int preferredInput,
            int preferredOutput, Band1CheckType band1CheckType, float band1Threshold,
            float balance) {
        this.name = name;
        this.sourceResId = sourceResId;
        this.inputPreset = inputPreset;
        this.anchors = anchors;
        this.bands = bands;
        this.balance = balance;
        this.passThreshold = passThreshold;
        this.preferredInput = preferredInput;
        this.preferredOutput = preferredOutput;
        this.band1CheckType = band1CheckType;
        this.band1Threshold = band1Threshold;
    }

    public FrequencyPreset(String name, int sourceResId, int inputPreset, int[] anchors,
            List<FrequencyBandSpec.BandThreshold> bands, float passThreshold, int preferredInput,
            int preferredOutput, Band1CheckType band1CheckType, float band1Threshold) {
        this(name, sourceResId, inputPreset, anchors, bands, passThreshold, preferredInput,
                preferredOutput, band1CheckType, band1Threshold, 0.5f);
    }

    public FrequencyPreset(String name, int sourceResId, int inputPreset, int[] anchors,
            List<FrequencyBandSpec.BandThreshold> bands, float passThreshold, int preferredInput,
            int preferredOutput) {
        this(name, sourceResId, inputPreset, anchors, bands, passThreshold, preferredInput,
                preferredOutput, Band1CheckType.NONE, 0.0f, 0.5f);
    }

}
