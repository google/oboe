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

import android.media.AudioDeviceInfo;
import java.util.ArrayList;
import java.util.List;

public class FrequencyPresetRepository {

    public static final int GROUP_FREQUENCY = 0;
    public static final int GROUP_DUAL = 1;

    private final List<FrequencyPreset> mPresets = new ArrayList<>();

    public FrequencyPresetRepository(int group) {
        if (group == GROUP_DUAL) {
            setupDualPresets();
        } else {
            setupFrequencyPresets();
        }
    }

    public List<FrequencyPreset> getPresets() {
        return mPresets;
    }

    private void setupFrequencyPresets() {
        // 1. Background Test
        List<FrequencyBandSpec.BandThreshold> bands1 = new ArrayList<>();
        bands1.add(new FrequencyBandSpec.BandThreshold(50.0f, 20.0f, -50.0f, -50.0f));
        bands1.add(new FrequencyBandSpec.BandThreshold(6.0f, 6.0f, -50.0f, -50.0f));
        bands1.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -50.0f, -50.0f));
        bands1.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -50.0f, -50.0f));
        mPresets.add(new FrequencyPreset("Background Test",
                R.string.source_silence, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{50, 100, 4000, 12000, 20000}, bands1, 30.0f,
                AudioDeviceInfo.TYPE_UNKNOWN, AudioDeviceInfo.TYPE_UNKNOWN,
                FrequencyPreset.Band1CheckType.LESS_THAN, -60.0f));
        // 2. Loopback Dongle
        List<FrequencyBandSpec.BandThreshold> bands2 = new ArrayList<>();
        bands2.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -50.0f, -4.0f));
        bands2.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -4.0f, -4.0f));
        bands2.add(new FrequencyBandSpec.BandThreshold(4.0f, 5.0f, -4.0f, -5.0f));
        bands2.add(new FrequencyBandSpec.BandThreshold(5.0f, 5.0f, -5.0f, -30.0f));
        mPresets.add(new FrequencyPreset("Loopback Dongle",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_UNPROCESSED,
                new int[]{50, 500, 4000, 12000, 20000}, bands2, 30.0f,
                AudioDeviceInfo.TYPE_USB_HEADSET, AudioDeviceInfo.TYPE_USB_HEADSET,
                FrequencyPreset.Band1CheckType.GREATER_THAN, -20.0f));

        // 3. Unprocessed Tone test
        List<FrequencyBandSpec.BandThreshold> bands3 = new ArrayList<>();
        bands3.add(new FrequencyBandSpec.BandThreshold(-6.0f, -6.0f, -100.0f, -100.0f));
        bands3.add(new FrequencyBandSpec.BandThreshold(10.0f, 10.0f, -50.0f, -10.0f));
        bands3.add(new FrequencyBandSpec.BandThreshold(-30.0f, -30.0f, -120.0f, -120.0f));
        mPresets.add(new FrequencyPreset("Unprocessed Tone test",
                R.string.source_sine, StreamConfiguration.INPUT_PRESET_UNPROCESSED,
                new int[]{30, 900, 1100, 20000}, bands3, 50.0f,
                AudioDeviceInfo.TYPE_BUILTIN_MIC, AudioDeviceInfo.TYPE_USB_DEVICE));

        // 4. Voice recognition tone test
        List<FrequencyBandSpec.BandThreshold> bands4 = new ArrayList<>();
        bands4.add(new FrequencyBandSpec.BandThreshold(-6.0f, -6.0f, -100.0f, -100.0f));
        bands4.add(new FrequencyBandSpec.BandThreshold(10.0f, 10.0f, -50.0f, -10.0f));
        bands4.add(new FrequencyBandSpec.BandThreshold(-30.0f, -30.0f, -120.0f, -120.0f));
        mPresets.add(new FrequencyPreset("Voice Recognition Tone test",
                R.string.source_sine, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{5, 900, 1100, 20000}, bands4, 50.0f,
                AudioDeviceInfo.TYPE_BUILTIN_MIC, AudioDeviceInfo.TYPE_USB_DEVICE));

        // 5. Built-in Mic and External Speaker
        List<FrequencyBandSpec.BandThreshold> bands5 = new ArrayList<>();
        bands5.add(new FrequencyBandSpec.BandThreshold(20.0f, 20.0f, -20.0f, -20.0f));
        bands5.add(new FrequencyBandSpec.BandThreshold(6.0f, 6.0f, -6.0f, -6.0f));
        bands5.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -30.0f, -30.0f));
        bands5.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -30.0f, -30.0f));
        mPresets.add(new FrequencyPreset("Built-in Mic and External Speaker",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{30, 100, 4000, 12000, 20000}, bands5, 30.0f,
                AudioDeviceInfo.TYPE_BUILTIN_MIC, AudioDeviceInfo.TYPE_USB_DEVICE,
                FrequencyPreset.Band1CheckType.GREATER_THAN, -50.0f));

        // 6. External Mic and External Speaker
        mPresets.add(new FrequencyPreset("External Mic and External Speaker",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{30, 100, 4000, 12000, 20000}, bands5, 30.0f,
                AudioDeviceInfo.TYPE_USB_DEVICE, AudioDeviceInfo.TYPE_USB_DEVICE,
                FrequencyPreset.Band1CheckType.GREATER_THAN, -50.0f));

        // 7. External Mic and Left Built-in Speaker
        List<FrequencyBandSpec.BandThreshold> bands7 = new ArrayList<>();
        bands7.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -50.0f, -4.0f));
        bands7.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -4.0f, -4.0f));
        bands7.add(new FrequencyBandSpec.BandThreshold(4.0f, 10.0f, -4.0f, -10.0f));
        bands7.add(new FrequencyBandSpec.BandThreshold(10.0f, 10.0f, -10.0f, -40.0f));
        mPresets.add(new FrequencyPreset("External Mic and Left Built-in Speaker",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{50, 500, 4000, 12000, 20000}, bands7, 30.0f,
                AudioDeviceInfo.TYPE_USB_DEVICE, AudioDeviceInfo.TYPE_BUILTIN_SPEAKER,
                FrequencyPreset.Band1CheckType.GREATER_THAN, -50.0f, 0.0f));

        // 8. External Mic and Right Built-in Speaker
        mPresets.add(new FrequencyPreset("External Mic and Right Built-in Speaker",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{50, 500, 4000, 12000, 20000}, bands7, 30.0f,
                AudioDeviceInfo.TYPE_USB_DEVICE, AudioDeviceInfo.TYPE_BUILTIN_SPEAKER,
                FrequencyPreset.Band1CheckType.GREATER_THAN, -50.0f, 1.0f));
    }

    private void setupDualPresets() {
        List<FrequencyBandSpec.BandThreshold> bandsUnprocessed = new ArrayList<>();
        bandsUnprocessed.add(new FrequencyBandSpec.BandThreshold(20.0f, 20.0f, -20.0f, -20.0f));
        bandsUnprocessed.add(new FrequencyBandSpec.BandThreshold(10.0f, 10.0f, -10.0f, -10.0f));
        bandsUnprocessed.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -30.0f, -30.0f));
        mPresets.add(new FrequencyPreset("Unprocessed",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_UNPROCESSED,
                new int[]{30, 100, 7000, 20000}, bandsUnprocessed, 50.0f,
                AudioDeviceInfo.TYPE_UNKNOWN, AudioDeviceInfo.TYPE_USB_DEVICE));

        List<FrequencyBandSpec.BandThreshold> bandsVoiceRec = new ArrayList<>();
        bandsVoiceRec.add(new FrequencyBandSpec.BandThreshold(20.0f, 20.0f, -20.0f, -20.0f));
        bandsVoiceRec.add(new FrequencyBandSpec.BandThreshold(6.0f, 6.0f, -6.0f, -6.0f));
        bandsVoiceRec.add(new FrequencyBandSpec.BandThreshold(30.0f, 30.0f, -30.0f, -30.0f));
        mPresets.add(new FrequencyPreset("Voice Recognition",
                R.string.source_white_noise, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{30, 100, 4000, 20000}, bandsVoiceRec, 50.0f,
                AudioDeviceInfo.TYPE_UNKNOWN, AudioDeviceInfo.TYPE_USB_DEVICE));

        mPresets.add(new FrequencyPreset("Silence Unprocessed",
                R.string.source_silence, StreamConfiguration.INPUT_PRESET_UNPROCESSED,
                new int[]{30, 100, 7000, 20000}, bandsUnprocessed, 50.0f,
                AudioDeviceInfo.TYPE_UNKNOWN, AudioDeviceInfo.TYPE_UNKNOWN));

        mPresets.add(new FrequencyPreset("Silence Voice Recognition",
                R.string.source_silence, StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
                new int[]{30, 100, 4000, 20000}, bandsVoiceRec, 50.0f,
                AudioDeviceInfo.TYPE_UNKNOWN, AudioDeviceInfo.TYPE_UNKNOWN));
    }
}
