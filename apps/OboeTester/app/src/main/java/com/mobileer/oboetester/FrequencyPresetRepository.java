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

import java.util.ArrayList;
import java.util.List;

public class FrequencyPresetRepository {
    private final List<FrequencyPreset> mPresets = new ArrayList<>();

    public FrequencyPresetRepository() {
        setupPresets();
    }

    public List<FrequencyPreset> getPresets() {
        return mPresets;
    }

    private void setupPresets() {
        // 1. Loopback Dongle
        List<FrequencyBandSpec.BandThreshold> bands1 = new ArrayList<>();
        bands1.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -50.0f, -4.0f));
        bands1.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -4.0f, -4.0f));
        bands1.add(new FrequencyBandSpec.BandThreshold(4.0f, 5.0f, -4.0f, -5.0f));
        bands1.add(new FrequencyBandSpec.BandThreshold(5.0f, 5.0f, -5.0f, -30.0f));
        mPresets.add(new FrequencyPreset("Loopback Dongle", R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_UNPROCESSED,
                new int[]{50, 500, 4000, 12000, 20000}, bands1));

        // 2. Built-in Mic and External Speaker
        List<FrequencyBandSpec.BandThreshold> bands2 = new ArrayList<>();
        bands2.add(new FrequencyBandSpec.BandThreshold(20.0f, 20.0f, -20.0f, -20.0f));
        bands2.add(new FrequencyBandSpec.BandThreshold(6.0f, 6.0f, -6.0f, -6.0f));
        bands2.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -30.0f, -30.0f));
        bands2.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -30.0f, -30.0f));
        mPresets.add(new FrequencyPreset("Built-in Mic and External Speaker", R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{30, 100, 4000, 12000, 20000}, bands2));

        // 3. External Mic and External Speaker
        mPresets.add(new FrequencyPreset("External Mic and External Speaker", R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{30, 100, 4000, 12000, 20000}, bands2)); // Same as above

        // 4. External Mic and Left Built-in Speaker
        List<FrequencyBandSpec.BandThreshold> bands4 = new ArrayList<>();
        bands4.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -50.0f, -4.0f));
        bands4.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -4.0f, -4.0f));
        bands4.add(new FrequencyBandSpec.BandThreshold(4.0f, 10.0f, -4.0f, -10.0f));
        bands4.add(new FrequencyBandSpec.BandThreshold(10.0f, 10.0f, -10.0f, -40.0f));
        mPresets.add(new FrequencyPreset("External Mic and Left Built-in Speaker", R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{50, 500, 4000, 12000, 20000}, bands4, 0.0f));

        // 5. External Mic and Right Built-in Speaker
        mPresets.add(new FrequencyPreset("External Mic and Right Built-in Speaker", R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{50, 500, 4000, 12000, 20000}, bands4, 1.0f));

        // 6. Background Test
        List<FrequencyBandSpec.BandThreshold> bands5 = new ArrayList<>();
        bands5.add(new FrequencyBandSpec.BandThreshold(50.0f, 20.0f, -50.0f, -50.0f));
        bands5.add(new FrequencyBandSpec.BandThreshold(6.0f, 6.0f, -50.0f, -50.0f));
        bands5.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -50.0f, -50.0f));
        bands5.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -50.0f, -50.0f));
        mPresets.add(new FrequencyPreset("Background Test", R.string.source_silence, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{50, 100, 4000, 12000, 20000}, bands5));
    }
}
