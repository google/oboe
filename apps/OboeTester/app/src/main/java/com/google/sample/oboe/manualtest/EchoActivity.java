/*
 * Copyright 2018 The Android Open Source Project
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
import android.view.View;

/**
 * Activity to record and play back audio.
 */
public class EchoActivity extends TestInputActivity {

    AudioOutputTester mAudioOutTester;

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_echo);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        updateEnabledWidgets();

        mAudioOutTester = addAudioOutputTester();
    }

    @Override
    protected void onStart() {
        super.onStart();
        setActivityType(ACTIVITY_ECHO);
    }

    public void onStartEcho(View view) {
        openAudio();
        startAudio();
    }

    public void onStopEcho(View view) {
        stopAudio();
        closeAudio();
    }

    @Override
    boolean isOutput() {
        return false;
    }

    @Override
    public void setupEffects(int sessionId) {
    }
}
