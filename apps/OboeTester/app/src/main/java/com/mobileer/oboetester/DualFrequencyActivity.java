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
import android.media.AudioDeviceInfo;
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
    private TextView mTestStatusView;
    private TextView mTestResultView;
    private FrequencyThresholdView mSubtractedTopThreshold;
    private FrequencyThresholdView mSubtractedBottomThreshold;
    private FrequencySetting mFrequencySetting;
    private StreamConfigurationView mInputConfigView;
    private FrequencyAnalyzer mFrequencyAnalyzer = new FrequencyAnalyzer();

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
        mTestStatusView = findViewById(R.id.testStatusView);
        mTestResultView = findViewById(R.id.testResultView);

        mSubtractedTopThreshold = findViewById(R.id.waveform_subtraction_top_threshold);
        mSubtractedTopThreshold.setDbfsRange(-50.0f, 50.0f);
        mSubtractedTopThreshold.updateTheme(
                android.graphics.Color.parseColor("#FFE91E63"),
                android.graphics.Color.TRANSPARENT,
                android.graphics.Color.RED);

        mSubtractedBottomThreshold = findViewById(R.id.waveform_subtraction_bottom_threshold);
        mSubtractedBottomThreshold.setDbfsRange(-50.0f, 50.0f);
        mSubtractedBottomThreshold.updateTheme(
                android.graphics.Color.parseColor("#FF4CAF50"),
                android.graphics.Color.TRANSPARENT,
                android.graphics.Color.RED);

        mInputConfigView = null;
        StreamConfigurationView outputConfigView = null;
        for (TestAudioActivity.StreamContext context : mStreamContexts) {
            if (context.isInput() && context.configurationView != null) {
                mInputConfigView = context.configurationView;
            } else if (!context.isInput() && context.configurationView != null) {
                outputConfigView = context.configurationView;
                outputConfigView.hideSettingsView();
            }
        }
        if (mInputConfigView != null) {
            mInputConfigView.hideSettingsView();
        }
        mFrequencySetting = new FrequencySetting(this,
                FrequencyPresetRepository.GROUP_DUAL,
                findViewById(R.id.radioGroupBands),
                findViewById(R.id.bandSpecContainer),
                findViewById(R.id.preset_spinner),
                new FrequencySetting.OnSettingChangedListener() {
                    @Override
                    public void onSettingChanged() {
                        FrequencyPreset active = mFrequencySetting.getActivePreset();
                        if (active != null) {
                            if (mInputConfigView != null) {
                                mInputConfigView.setInputPreset(active.inputPreset);
                            }
                            mAudioOutTester.setSignalType(
                                    FrequencySetting.getSignalIndexForSource(active.sourceResId));
                        }
                    }
                });

        mWaveformViewTest1 = findViewById(R.id.waveform_test1);
        mWaveformViewTest1.setDbfsRange(MIN_DBFS, MAX_DBFS);

        mWaveformViewTest2 = findViewById(R.id.waveform_test2);
        mWaveformViewTest2.setDbfsRange(MIN_DBFS, MAX_DBFS);
        // Overlap styling for View 1 Test 2: transparent background with green line to contrast with blue Test 1
        mWaveformViewTest2.updateTheme(Color.GREEN, Color.TRANSPARENT, Color.GREEN);

        mWaveformViewSubtraction = findViewById(R.id.waveform_subtraction);
        mWaveformViewSubtraction.setDbfsRange(-50.0f, 50.0f);
        // Keep line red, but use normal background since they're not overlapping anymore
        mWaveformViewSubtraction.updateTheme(Color.RED,
                getResources().getColor(R.color.waveform_background), Color.RED);

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
            if (mInputConfigView != null) {
                AudioDeviceInfo device = findInputDeviceByType(AudioDeviceInfo.TYPE_BUILTIN_MIC);
                if (device != null) {
                    mInputConfigView.setDeviceById(device.getId());
                } else {
                    mInputConfigView.setDeviceById(0); // Auto-select
                    showToast("WARNING: Preferred input device (BUILTIN_MIC) not found!");
                }
            }
            checkPreferredOutput();
            openAudio();
            startAudio();
            mStartButton1.setEnabled(false);
            mStopButton1.setEnabled(true);
            startWaveformUpdater();
            keepScreenOn(true);
            mTestStatusView.setText("Status: Running Test 1...");
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
        mTestStatusView.setText("Status: Test 1 Stopped. Ready for Test 2.");
    }

    public void onStartTest2(View view) {
        try {
            mCurrentTest = 2;
            if (mInputConfigView != null) {
                AudioDeviceInfo device = findInputDeviceByType(AudioDeviceInfo.TYPE_USB_DEVICE);
                if (device != null) {
                    mInputConfigView.setDeviceById(device.getId());
                } else {
                    mInputConfigView.setDeviceById(0); // Auto-select
                    showToast("WARNING: Preferred input device (USB_DEVICE) not found!");
                }
            }
            checkPreferredOutput();
            openAudio();
            startAudio();
            mStartButton2.setEnabled(false);
            mStopButton2.setEnabled(true);
            mStartButton1.setEnabled(false);
            startWaveformUpdater();
            keepScreenOn(true);
            mTestStatusView.setText("Status: Running Test 2...");
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
        mTestStatusView.setText("Status: Test 2 Stopped. Tests complete.");
    }

    private void checkPreferredOutput() {
        FrequencyPreset active = mFrequencySetting.getActivePreset();
        if (active != null) {
            int preferredOutput = active.preferredOutput;
            if (preferredOutput != AudioDeviceInfo.TYPE_UNKNOWN) {
                int result = checkOutputDeviceSupported(preferredOutput);
                if (result == DEVICE_NOT_FOUND) {
                    showToast("WARNING: Preferred output device (" +
                            StreamConfiguration.deviceTypeToString(preferredOutput) +
                            ") not found!");
                } else if (result == DEVICE_CONFLICT_USB_PLUGGED) {
                    showToast("WARNING: USB device is plugged in while testing Built-in Speaker!");
                }
            }
        }
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
            if (!mIsUpdaterRunning) {
                return;
            }
            try {
                int numSamples = getFftMagnitude(mWaveformBuffer);
                if (numSamples > 0) {
                    if (mCurrentTest == 1) {
                        mWaveformViewTest1.setSampleData(mWaveformBuffer, 0, numSamples);
                        mWaveformViewTest1.postInvalidate();
                    } else if (mCurrentTest == 2) {
                        // 1. Draw live Test 2 FFT overlapped on the top graph
                        mWaveformViewTest2.setSampleData(mWaveformBuffer, 0, numSamples);
                        mWaveformViewTest2.postInvalidate();

                        // 2. Draw the subtracted FFT on the bottom graph
                        if (mTest1WaveformBuffer != null) {
                            float[] rawDiffBuffer = new float[numSamples];
                            for (int i = 0; i < numSamples; i++) {
                                rawDiffBuffer[i] = mTest1WaveformBuffer[i] - mWaveformBuffer[i];
                            }
                            mWaveformViewSubtraction.setSampleData(rawDiffBuffer, 0, numSamples);
                            mWaveformViewSubtraction.postInvalidate();

                            // Run Java Analyzer on subtracted FFT
                            float[] frequencies = new float[numSamples];
                            int numFreqs = getFftFrequencies(frequencies);

                            FrequencyBandSpec spec =
                                    mFrequencySetting != null ? mFrequencySetting.getSpec() : null;
                            float passThreshold = 50.0f; // failure rate 50%

                            FrequencyAnalyzer.AnalysisResult result = mFrequencyAnalyzer.analyze(
                                    rawDiffBuffer, numSamples, frequencies, numFreqs, spec,
                                    passThreshold, true);

                            if (result != null) {
                                if (numFreqs > 0) {
                                    mSubtractedTopThreshold.setMaxFrequency(
                                            frequencies[numFreqs - 1]);
                                    mSubtractedBottomThreshold.setMaxFrequency(
                                            frequencies[numFreqs - 1]);
                                }

                                mSubtractedTopThreshold.setFrequencies(result.thresholdFrequencies);
                                mSubtractedBottomThreshold.setFrequencies(
                                        result.thresholdFrequencies);

                                mSubtractedTopThreshold.setSampleData(
                                        result.alignedTopThresholdsDbfs);
                                mSubtractedTopThreshold.postInvalidate();
                                mSubtractedBottomThreshold.setSampleData(
                                        result.alignedBottomThresholdsDbfs);
                                mSubtractedBottomThreshold.postInvalidate();

                                StringBuilder sb = new StringBuilder();
                                sb.append("RESULT: ").append(result.testPassed ? "PASS" : "FAIL")
                                        .append("\n");
                                sb.append("Bands: ");
                                for (int b = 0; b < result.bandEnergyPercentages.length; b++) {
                                    sb.append(String.format(java.util.Locale.getDefault(),
                                            "[B%d: %.1f%%] ", b, result.bandEnergyPercentages[b]));
                                }
                                mTestResultView.setText(sb.toString());
                                mTestResultView.setTextColor(
                                        result.testPassed ? android.graphics.Color.parseColor(
                                                "#FF4CAF50") : android.graphics.Color.RED);
                            }
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

}
