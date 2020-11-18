/*
 * Copyright 2020 The Android Open Source Project
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

package com.google.sample.oboe.manualtest;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.IOException;
import java.util.Date;

public class TestDataPathsActivity  extends BaseAutoGlitchActivity {

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_data_paths);
    }

    private static final int[] INPUT_PRESETS = {
            StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
            StreamConfiguration.INPUT_PRESET_GENERIC,
            StreamConfiguration.INPUT_PRESET_CAMCORDER,
            StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
            StreamConfiguration.INPUT_PRESET_UNPROCESSED,
            StreamConfiguration.INPUT_PRESET_VOICE_PERFORMANCE,
    };

    protected String getConfigText(StreamConfiguration config) {
        return (super.getConfigText(config)
                + ", " + ((config.getDirection() == StreamConfiguration.DIRECTION_INPUT) ?
                    (" inPre = " + config.getInputPreset()) : ""));
    }

    void testInputPreset(int inputPreset) throws InterruptedException {
        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();

        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);

        requestedInConfig.setInputPreset(inputPreset);
        setTolerance(0.5f);

        requestedInConfig.setChannelCount(1);
        requestedOutConfig.setChannelCount(1);

        testConfigurations();
    }

    @Override
    public void runTest() {
        try {
            mDurationSeconds = 2;
            for (int inputPreset : INPUT_PRESETS) {
                testInputPreset(inputPreset);
            }
        } catch (Exception e) {
            //log(e.getMessage());
            showErrorToast(e.getMessage());
        }
    }

}
