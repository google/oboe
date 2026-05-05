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

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import java.io.IOException;

public final class FrequencyActivity extends AnalyzerActivity {
    private Button mStartButton;
    private Button mStopButton;
    private Button mShareButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mShareButton = (Button) findViewById(R.id.button_share);

        mStopButton.setEnabled(false);
        mShareButton.setEnabled(false);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_frequency);
    }

    @Override
    String getWaveTag() {
        return "frequency";
    }

    @Override
    int getActivityType() {
        return ACTIVITY_FREQUENCY;
    }

    @Override
    boolean isOutput() {
        return true;
    }

    public void onStartAudioTest(View view) {
        try {
            // In NativeAudioContext.h, WhiteNoise is index 4
            mAudioOutTester.setSignalType(4); 
            openAudio();
            startAudio();
            mStartButton.setEnabled(false);
            mStopButton.setEnabled(true);
            mShareButton.setEnabled(false);
            keepScreenOn(true);
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onStopAudioTest(View view) {
        stopAudio();
        closeAudio();
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mShareButton.setEnabled(true);
        keepScreenOn(false);
    }
}
