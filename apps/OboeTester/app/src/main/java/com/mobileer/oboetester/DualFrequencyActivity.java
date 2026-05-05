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

import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import java.io.IOException;

public class DualFrequencyActivity extends AnalyzerActivity {

    private Button mStartButton1, mStopButton1;
    private Button mStartButton2, mStopButton2;
    private FftWaveformView mWaveformViewTest1, mWaveformViewSubtraction, mWaveformViewTest2;
    private TextView mTestResultView;

    private static final int WAVEFORM_UPDATE_MS = 500;
    private static final float MIN_DBFS = -100.0f;
    private static final float MAX_DBFS = 0.0f;

    private float[] mWaveformBuffer;
    private float[] mTest1WaveformBuffer;
    private boolean mIsUpdaterRunning = false;
    private int mCurrentTest = 0; // 1 for test 1, 2 for test 2

    private Handler mHandler = new Handler();

    private native int getWindowSize();
    private native int getFftMagnitude(float[] waveform);
    private native int getFftFrequencies(float[] frequencies);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mStartButton1 = findViewById(R.id.button_start_1);
        mStopButton1 = findViewById(R.id.button_stop_1);
        mStartButton2 = findViewById(R.id.button_start_2);
        mStopButton2 = findViewById(R.id.button_stop_2);
        mTestResultView = findViewById(R.id.testResultView);

        mWaveformViewTest1 = findViewById(R.id.waveform_test1);
        mWaveformViewTest1.setDbfsRange(MIN_DBFS, MAX_DBFS);

        mWaveformViewTest2 = findViewById(R.id.waveform_test2);
        mWaveformViewTest2.setDbfsRange(MIN_DBFS, MAX_DBFS);
        // Overlap styling for View 1 Test 2: transparent background with green line to contrast with blue Test 1
        mWaveformViewTest2.updateTheme(Color.GREEN, Color.TRANSPARENT, Color.GREEN);

        mWaveformViewSubtraction = findViewById(R.id.waveform_subtraction);
        mWaveformViewSubtraction.setDbfsRange(-50.0f, 50.0f);
        // Keep line red, but use normal background since they're not overlapping anymore
        mWaveformViewSubtraction.updateTheme(Color.RED, getResources().getColor(R.color.waveform_background), Color.RED);

        mStopButton1.setEnabled(false);
        mStartButton2.setEnabled(false);
        mStopButton2.setEnabled(false);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_dual_frequency);
    }

    @Override
    String getWaveTag() {
        return "dual_frequency";
    }

    @Override
    int getActivityType() {
        return ACTIVITY_DUAL_FREQUENCY;
    }

    @Override
    boolean isOutput() {
        return true;
    }

    public void onStartTest1(View view) {
        try {
            mCurrentTest = 1;
            openAudio();
            startAudio();
            mStartButton1.setEnabled(false);
            mStopButton1.setEnabled(true);
            startWaveformUpdater();
            keepScreenOn(true);
            mTestResultView.setText("Status: Running Test 1...");
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onStopTest1(View view) {
        stopWaveformUpdater();
        stopAudio();
        closeAudio();
        mStartButton1.setEnabled(true);
        mStopButton1.setEnabled(false);
        mStartButton2.setEnabled(true);
        keepScreenOn(false);

        // Save the current buffer for subtraction
        if (mWaveformBuffer != null) {
            mTest1WaveformBuffer = mWaveformBuffer.clone();
        }
        mTestResultView.setText("Status: Test 1 Stopped. Ready for Test 2.");
    }

    public void onStartTest2(View view) {
        try {
            mCurrentTest = 2;
            openAudio();
            startAudio();
            mStartButton2.setEnabled(false);
            mStopButton2.setEnabled(true);
            mStartButton1.setEnabled(false);
            startWaveformUpdater();
            keepScreenOn(true);
            mTestResultView.setText("Status: Running Test 2...");
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onStopTest2(View view) {
        stopWaveformUpdater();
        stopAudio();
        closeAudio();
        mStartButton2.setEnabled(true);
        mStopButton2.setEnabled(false);
        mStartButton1.setEnabled(true);
        keepScreenOn(false);
        mTestResultView.setText("Status: Test 2 Stopped. Tests complete.");
    }

    private void startWaveformUpdater() {
        if (!mIsUpdaterRunning) {
            mWaveformBuffer = new float[getWindowSize()];
            mIsUpdaterRunning = true;
            mHandler.postDelayed(mWaveformUpdater, WAVEFORM_UPDATE_MS);
        }
    }

    private void stopWaveformUpdater() {
        if (mIsUpdaterRunning) {
            mIsUpdaterRunning = false;
            mHandler.removeCallbacks(mWaveformUpdater);
        }
    }

    private Runnable mWaveformUpdater = new Runnable() {
        @Override
        public void run() {
            if (!mIsUpdaterRunning) return;
            try {
                int numSamples = getFftMagnitude(mWaveformBuffer);
                if (numSamples > 0) {
                    if (mCurrentTest == 1) {
                        float[] samplesToDraw = new float[numSamples];
                        for (int i = 0; i < numSamples; i++) {
                            samplesToDraw[i] = mapDbfsToView(mWaveformBuffer[i]);
                        }
                        mWaveformViewTest1.setSampleData(samplesToDraw);
                        mWaveformViewTest1.postInvalidate();
                    } else if (mCurrentTest == 2) {
                        // 1. Draw live Test 2 FFT overlapped on the top graph
                        float[] samplesToDraw = new float[numSamples];
                        for (int i = 0; i < numSamples; i++) {
                            samplesToDraw[i] = mapDbfsToView(mWaveformBuffer[i]);
                        }
                        mWaveformViewTest2.setSampleData(samplesToDraw);
                        mWaveformViewTest2.postInvalidate();

                        // 2. Draw the subtracted FFT on the bottom graph
                        if (mTest1WaveformBuffer != null) {
                            float[] subtractedSamples = new float[numSamples];
                            for (int i = 0; i < numSamples; i++) {
                                float diff = mTest1WaveformBuffer[i] - mWaveformBuffer[i];
                                subtractedSamples[i] = mapSubtractedDbfsToView(diff);
                            }
                            mWaveformViewSubtraction.setSampleData(subtractedSamples);
                            mWaveformViewSubtraction.postInvalidate();
                        }
                    }
                }
            } finally {
                if (mIsUpdaterRunning) {
                    mHandler.postDelayed(this, WAVEFORM_UPDATE_MS);
                }
            }
        }
    };

    private float mapSubtractedDbfsToView(float diff) {
        // Map difference in range -50 to 50 dB
        float min = -50.0f;
        float max = 50.0f;
        float mapped = ((diff - min) / (max - min)) * 2.0f - 1.0f;
        if (mapped < -1.0f) mapped = -1.0f;
        if (mapped > 1.0f) mapped = 1.0f;
        return mapped;
    }

    private float mapDbfsToView(float dbfs) {
        float mapped = ((dbfs - MIN_DBFS) / (MAX_DBFS - MIN_DBFS)) * 2.0f - 1.0f;
        if (mapped < -1.0f) mapped = -1.0f;
        if (mapped > 1.0f) mapped = 1.0f;
        return mapped;
    }
}
