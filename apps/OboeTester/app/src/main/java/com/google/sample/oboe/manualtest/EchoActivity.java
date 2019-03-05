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

import android.view.View;

/**
 * Activity to record and play back audio.
 */
public class EchoActivity extends TestAudioActivity {

    private static final int STATE_RECORDING = 5;
    private static final int STATE_RUNNING = 6;
    private int mRecorderState = STATE_STOPPED;

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_echo);
    }

    public void onStartEcho(View view) {
        openAudio();
        startAudio();
        mRecorderState = STATE_RECORDING;
    }

    public void onStopEcho(View view) {
        stopAudio();
        closeAudio();
        mRecorderState = STATE_STOPPED;
    }

    public void startPlayback() {
        try {
            // mAudioStreamTester.startPlayback();
            updateStreamConfigurationViews();
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            showToast(e.getMessage());
        }

    }

    @Override
    boolean isOutput() {
        return false;
    }

    @Override
    public void setupEffects(int sessionId) {
    }
}
