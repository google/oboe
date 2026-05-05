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
import android.widget.TextView;
import android.widget.EditText;
import java.io.IOException;
import android.os.Handler;
import java.util.ArrayList;
import java.util.List;

public final class FrequencyActivity extends AnalyzerActivity {
    private Button mStartButton;
    private Button mStopButton;
    private Button mShareButton;
    private TextView mTestResultView;
    private EditText mThresholdEditText;
    private static final int WAVEFORM_UPDATE_MS = 500;

    private float[] mWaveformBuffer;
    private Handler mHandler = new Handler();
    private boolean mIsUpdaterRunning = false;

    private FrequencyAnalyzer mFrequencyAnalyzer = new FrequencyAnalyzer();
    private FrequencyBandSpec mLoopbackSpec;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mShareButton = (Button) findViewById(R.id.button_share);
        mTestResultView = (TextView) findViewById(R.id.testResultView);
        mThresholdEditText = (EditText) findViewById(R.id.thresholdEditText);

        mStopButton.setEnabled(false);
        mShareButton.setEnabled(false);

        // Initialize hardcoded Loopback Spec
        List<FrequencyBandSpec.BandThreshold> bands = new ArrayList<>();
        bands.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -50.0f, -4.0f));
        bands.add(new FrequencyBandSpec.BandThreshold(4.0f, 4.0f, -4.0f, -4.0f));
        bands.add(new FrequencyBandSpec.BandThreshold(4.0f, 5.0f, -4.0f, -5.0f));
        bands.add(new FrequencyBandSpec.BandThreshold(5.0f, 5.0f, -5.0f, -30.0f));
        mLoopbackSpec = new FrequencyBandSpec(new int[]{50, 500, 4000, 12000, 20000}, bands);
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
            mAudioOutTester.setSignalType(0);
            openAudio();
            startAudio();
            mStartButton.setEnabled(false);
            mStopButton.setEnabled(true);
            mShareButton.setEnabled(false);
            startWaveformUpdater();
            keepScreenOn(true);
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onStopAudioTest(View view) {
        stopWaveformUpdater();
        stopAudio();
        closeAudio();
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mShareButton.setEnabled(true);
        keepScreenOn(false);
    }

    private native int getWindowSize();
    private native int getFftMagnitude(float[] waveform);
    private native int getFftFrequencies(float[] frequencies);

    private Runnable mWaveformUpdater = new Runnable() {
        @Override
        public void run() {
            if (!mIsUpdaterRunning) return;
            try {
                int numSamples = getFftMagnitude(mWaveformBuffer);
                if (numSamples > 0) {
                    float[] frequencies = new float[numSamples];
                    int numFreqs = getFftFrequencies(frequencies);

                    float passThreshold = 30.0f;
                    try {
                        passThreshold = Float.parseFloat(mThresholdEditText.getText().toString());
                    } catch (NumberFormatException e) {
                        // Use default threshold
                    }

                    FrequencyAnalyzer.AnalysisResult result = mFrequencyAnalyzer.analyze(
                            mWaveformBuffer, numSamples, frequencies, numFreqs, mLoopbackSpec, passThreshold);
                    
                    if (result != null) {
                        StringBuilder sb = new StringBuilder();
                        sb.append("RESULT: ").append(result.testPassed ? "PASS" : "FAIL").append("\n");
                        sb.append("Bands: ");
                        for (int b = 0; b < result.bandEnergyPercentages.length; b++) {
                            sb.append(String.format(java.util.Locale.getDefault(), "[B%d: %.1f%%] ", b, result.bandEnergyPercentages[b]));
                        }
                        mTestResultView.setText(sb.toString());
                        mTestResultView.setTextColor(result.testPassed ? android.graphics.Color.parseColor("#FF4CAF50") : android.graphics.Color.RED);
                    }
                }
            } finally {
                if (mIsUpdaterRunning) {
                    mHandler.postDelayed(this, WAVEFORM_UPDATE_MS);
                }
            }
        }
    };

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
}
