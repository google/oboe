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

import android.content.Context;
import android.graphics.Color;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;
import java.io.IOException;

public class DualFrequencyActivity extends AnalyzerActivity {

    private Button mStartButton1, mStopButton1;
    private Button mStartButton2, mStopButton2;
    private CheckBox mEnforcementCheckBox;
    private LineView mWaveformViewTests;
    private int mTest1LineId = -1;
    private int mTest2LineId = -1;

    private LineView mWaveformViewSubtraction;
    private int mSubtractionLineId = -1;
    private int mTopThresholdLineId = -1;
    private int mBottomThresholdLineId = -1;
    private TextView mTestStatusView;
    private TextView mTestResultView;
    private TextView mInstructionsView;
    private FrequencySettingView mFrequencySetting;
    private StreamConfigurationView mInputConfigView;
    private TestSupervisor mTestSupervisor;
    private FrequencyAnalyzer mFrequencyAnalyzer = new FrequencyAnalyzer();

    private static final int WAVEFORM_UPDATE_MS = 500;
    private static final float MIN_DBFS = -100.0f;
    private static final float MAX_DBFS = 0.0f;

    private float[] mWaveformBuffer;
    private float[] mTest1WaveformBuffer;
    private boolean mIsUpdaterRunning = false;
    private int mCurrentTest = 0; // 1 for test 1, 2 for test 2

    private Handler mHandler = new Handler();
    private int[] mPreferredInputList = {AudioDeviceInfo.TYPE_BUILTIN_MIC, AudioDeviceInfo.TYPE_USB_DEVICE};

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
        mInstructionsView = findViewById(R.id.test_dual_frequency_instructions);
        mEnforcementCheckBox = findViewById(R.id.enforcement_check_box);

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
        mFrequencySetting = findViewById(R.id.frequency_setting);
        mFrequencySetting.initialize(FrequencyPresetRepository.GROUP_DUAL,
                new FrequencySettingView.OnSettingChangedListener() {
                    @Override
                    public void onSettingChanged() {
                        FrequencyPreset active = mFrequencySetting.getActivePreset();
                        if (active != null) {
                            if (mInputConfigView != null) {
                                mInputConfigView.setInputPreset(active.inputPreset);
                            }
                            mAudioOutTester.setSignalType(
                                    FrequencySettingView.getSignalIndexForSource(
                                            active.sourceResId));
                            if (mInstructionsView != null) {
                                mInstructionsView.setText(active.instructionsResId);
                            }
                        }
                    }
                });

        mWaveformViewTests = findViewById(R.id.waveform_view_tests);
        mWaveformViewTests.setUnits("Hz", "dBFS");
        mWaveformViewTests.setYRange(MIN_DBFS, MAX_DBFS);
        mWaveformViewTests.setLogScaleX(true);
        mWaveformViewTests.setGridLinesY(
                new float[]{MIN_DBFS, (MIN_DBFS + MAX_DBFS) / 2.0f, MAX_DBFS});

        mWaveformViewSubtraction = findViewById(R.id.waveform_view_subtraction);
        mWaveformViewSubtraction.setUnits("Hz", "dBFS");
        mWaveformViewSubtraction.setYRange(-50.0f, 50.0f);
        mWaveformViewSubtraction.setLogScaleX(true);
        mWaveformViewSubtraction.setGridLinesY(new float[]{-50.0f, 0.0f, 50.0f});

        android.widget.CheckBox logScaleCheckbox = findViewById(R.id.checkboxLogScale);
        logScaleCheckbox.setOnCheckedChangeListener(
                new android.widget.CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(android.widget.CompoundButton buttonView,
                            boolean isChecked) {
                        mWaveformViewTests.setLogScaleX(isChecked);
                        mWaveformViewSubtraction.setLogScaleX(isChecked);
                    }
                });

        mStopButton1.setEnabled(false);
        mStartButton2.setEnabled(false);
        mStopButton2.setEnabled(false);

        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        mTestSupervisor = new TestSupervisor(audioManager, AudioManager.STREAM_MUSIC);
    }

    @Override
    protected void onDestroy() {
        mTestSupervisor.release();
        super.onDestroy();
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

    public void onStartTest(View view) {
        boolean isEnforced = mEnforcementCheckBox.isChecked();
        if (view.getId() == mStartButton1.getId()) {
            mCurrentTest = 1;
        } else {
            mCurrentTest = 2;
        }
        FrequencyPreset activePreset = mFrequencySetting.getActivePreset();
        boolean inputReady = prepareInput(mPreferredInputList[mCurrentTest - 1]);
        boolean outputReady = prepareOutput(activePreset);
        boolean peripheralReady = inputReady && outputReady;

        if (isEnforced && !peripheralReady) {
            // Mark the test fail
            mTestResultView.setText("RESULT: FAIL (Missing Peripherals)");
            mTestResultView.setTextColor(Color.RED);
            return;
        } else if (!peripheralReady) {
            if (!inputReady) {
                showToast("WARNING: Preferred input device is not found!");
            }
            if (!outputReady) {
                showToast("WARNING: Preferred output device is not found!");
            }
        }
        if (isEnforced) {
            showMaxVolumeConfirmationDialog();
        } else {
            startTest();
        }
    }

    public void onStopTest(View view) {
        boolean isEnforced = mEnforcementCheckBox.isChecked();
        stopWaveformUpdater();
        stopAudio();
        closeAudio();
        keepScreenOn(false);
        if (view.getId() == mStopButton1.getId()) {
            mStartButton1.setEnabled(true);
            mStopButton1.setEnabled(false);
        } else {
            mStartButton1.setEnabled(true);
            mStartButton2.setEnabled(true);
            mStopButton2.setEnabled(false);
        }
        mTestSupervisor.stop();
        String events = checkSupervisorEvents();

        // Process events
        if (!events.isBlank()) {
            if(isEnforced) {
                // Override the result
                mTestResultView.setText("RESULT: FAIL (" + events + ")");
                mTestResultView.setTextColor(Color.RED);
            } else {
                // Show a toast
                showToast("WARNING: " + events + " during test!");
            }
        }
        // Update buttons
        if (events.isBlank() || !isEnforced) {
            if (view.getId() == mStopButton1.getId()) {
                // Allow to start the second test
                if (mWaveformBuffer != null) {
                    mTest1WaveformBuffer = mWaveformBuffer.clone();
                }
                mStartButton2.setEnabled(true);
            } else {
                // Do nothing
            }
        }
    }

    private void startTest() {
        if (mCurrentTest == 1) {
            clearResult();
        }
        mTestSupervisor.start();
        try {
            openAudio();
            startAudio();
            startWaveformUpdater();
            keepScreenOn(true);

            mStartButton2.setEnabled(false);
            mStartButton1.setEnabled(false);
            if (mCurrentTest == 1) {
                mStopButton1.setEnabled(true);
                mStopButton2.setEnabled(false);
            } else {
                mStopButton1.setEnabled(false);
                mStopButton2.setEnabled(true);
            }
            mTestStatusView.setText("Status: Running Test " + mCurrentTest);
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    private void clearResult() {
        mTestResultView.setText("Result: N/A");
        mTest1LineId = -1;
        mTest2LineId = -1;
        mSubtractionLineId = -1;
        mTopThresholdLineId = -1;
        mBottomThresholdLineId = -1;
        mWaveformViewTests.removeAllLines();
        mWaveformViewSubtraction.removeAllLines();
    }

    private void showMaxVolumeConfirmationDialog() {
        new android.app.AlertDialog.Builder(this)
                .setTitle("Volume Warning")
                .setMessage("High enforcement mode will set the volume to MAXIMUM. This may be very loud. Do you want to proceed?")
                .setPositiveButton("Proceed", new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        setVolumeToMax();
                        startTest();
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void setVolumeToMax() {
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audioManager != null) {
            int maxVolume = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, maxVolume, 0);
        }
    }

    private boolean prepareInput(int inputType) {
        AudioDeviceInfo device = findInputDeviceByType(inputType);
        if (device != null) {
            mInputConfigView.setDeviceById(device.getId());
        } else {
            mInputConfigView.setDeviceById(0); // Auto-select
        }
        return device != null;
    }

    private boolean prepareOutput(FrequencyPreset preset) {
        if (preset != null) {
            int preferredOutput = preset.preferredOutput;
            if (preferredOutput != AudioDeviceInfo.TYPE_UNKNOWN) {
                int result = checkOutputDeviceSupported(preferredOutput);
                if (result == DEVICE_NOT_FOUND) {
                    return false;
                } else if (result == DEVICE_CONFLICT_USB_PLUGGED) {
                    showToast("WARNING: USB device is plugged in while testing Built-in Speaker!");
                }
            }
        }
        return true;
    }

    private String checkSupervisorEvents() {
        String noError = "";
        boolean volChanged = mTestSupervisor.isVolumeChanged();
        boolean devChanged = mTestSupervisor.isDeviceChanged();
        if (volChanged || devChanged) {
            StringBuilder reason = new StringBuilder();
            if (volChanged) reason.append("Volume changed");
            if (devChanged) {
                if (reason.length() > 0) reason.append(" & ");
                reason.append("Device changed");
            }
            return reason.toString();
        }
        return noError;
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
                    float[] frequencies = new float[numSamples];
                    int numFreqs = getFftFrequencies(frequencies);

                    float minFreq = frequencies[0];
                    float maxFreq = frequencies[numFreqs - 1];
                    FrequencyBandSpec spec =
                            mFrequencySetting != null ? mFrequencySetting.getSpec() : null;
                    if (spec != null && spec.getFrequencyAnchors() != null
                            && spec.getFrequencyAnchors().length > 0) {
                        minFreq = spec.getFrequencyAnchors()[0];
                        int[] anchors = spec.getFrequencyAnchors();
                        float[] cursorFreqs = new float[anchors.length];
                        for (int i = 0; i < anchors.length; i++) {
                            cursorFreqs[i] = anchors[i];
                        }
                        mWaveformViewTests.setGridLinesX(cursorFreqs);
                        mWaveformViewSubtraction.setGridLinesX(cursorFreqs);
                    }
                    mWaveformViewTests.setXRange(minFreq, maxFreq);
                    mWaveformViewSubtraction.setXRange(minFreq, maxFreq);

                    if (mCurrentTest == 1) {
                        float[] yValues = java.util.Arrays.copyOf(mWaveformBuffer, numSamples);
                        if (mTest1LineId == -1) {
                            @SuppressWarnings("deprecation")
                            int color = getResources().getColor(R.color.waveform_line);
                            mTest1LineId = mWaveformViewTests.addLine(frequencies, yValues, color,
                                    LineView.DrawMode.BAR);
                        } else {
                            mWaveformViewTests.updateLine(mTest1LineId, frequencies, yValues);
                        }
                    } else if (mCurrentTest == 2) {
                        float[] yValues = java.util.Arrays.copyOf(mWaveformBuffer, numSamples);
                        if (mTest2LineId == -1) {
                            mTest2LineId = mWaveformViewTests.addLine(frequencies, yValues,
                                    android.graphics.Color.GREEN, LineView.DrawMode.BAR);
                        } else {
                            mWaveformViewTests.updateLine(mTest2LineId, frequencies, yValues);
                        }

                        if (mTest1WaveformBuffer != null) {
                            float[] rawDiffBuffer = new float[numSamples];
                            for (int i = 0; i < numSamples; i++) {
                                rawDiffBuffer[i] = mTest1WaveformBuffer[i] - mWaveformBuffer[i];
                            }
                            if (mSubtractionLineId == -1) {
                                mSubtractionLineId = mWaveformViewSubtraction.addLine(frequencies,
                                        rawDiffBuffer, android.graphics.Color.RED,
                                        LineView.DrawMode.BAR);
                            } else {
                                mWaveformViewSubtraction.updateLine(mSubtractionLineId, frequencies,
                                        rawDiffBuffer);
                            }

                            float passThreshold = 50.0f;
                            FrequencyAnalyzer.AnalysisResult result = mFrequencyAnalyzer.analyze(
                                    rawDiffBuffer, numSamples, frequencies, numFreqs, spec,
                                    passThreshold, true);

                            if (result != null) {
                                if (mTopThresholdLineId == -1) {
                                    mTopThresholdLineId = mWaveformViewSubtraction.addLine(
                                            result.thresholdFrequencies,
                                            result.alignedTopThresholdsDbfs,
                                            android.graphics.Color.parseColor("#FFE91E63"));
                                } else {
                                    mWaveformViewSubtraction.updateLine(mTopThresholdLineId,
                                            result.thresholdFrequencies,
                                            result.alignedTopThresholdsDbfs);
                                }

                                if (mBottomThresholdLineId == -1) {
                                    mBottomThresholdLineId = mWaveformViewSubtraction.addLine(
                                            result.thresholdFrequencies,
                                            result.alignedBottomThresholdsDbfs,
                                            android.graphics.Color.parseColor("#FF4CAF50"));
                                } else {
                                    mWaveformViewSubtraction.updateLine(mBottomThresholdLineId,
                                            result.thresholdFrequencies,
                                            result.alignedBottomThresholdsDbfs);
                                }

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
