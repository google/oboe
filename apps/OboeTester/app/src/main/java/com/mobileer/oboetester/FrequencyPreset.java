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
    public String name;
    public int sourceResId;
    public int inputPreset;
    public int[] anchors;
    public List<FrequencyBandSpec.BandThreshold> bands;
    public float balance = 0.5f;

    public FrequencyPreset(String name, int sourceResId, int inputPreset, int[] anchors, List<FrequencyBandSpec.BandThreshold> bands, float balance) {
        this.name = name;
        this.sourceResId = sourceResId;
        this.inputPreset = inputPreset;
        this.anchors = anchors;
        this.bands = bands;
        this.balance = balance;
    }

    public FrequencyPreset(String name, int sourceResId, int inputPreset, int[] anchors, List<FrequencyBandSpec.BandThreshold> bands) {
        this(name, sourceResId, inputPreset, anchors, bands, 0.5f);
    }
}
