/*
 * Copyright 2017 The Android Open Source Project
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

import android.os.Bundle;

import com.google.sample.oboe.manualtest.R;

/**
 * Base class for output test activities
 */
public class TestOutputActivity extends TestOutputActivityBase {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_test_output);

        findAudioCommon();
        updateEnabledWidgets();

        mAudioStreamTester = mAudioOutTester = AudioOutputTester.getInstance();
    }

    public void startAudio() {
        super.startAudio();
        mAudioOutTester.setToneType(OboeAudioOutputStream.TONE_TYPE_SINE_STEADY);
        mAudioOutTester.setEnabled(true);
    }

    public void stopAudio() {
        mAudioOutTester.setEnabled(false);
        super.stopAudio();
    }

}
