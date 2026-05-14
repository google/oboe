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
import android.media.AudioDeviceInfo;
import android.view.View;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.EditText;
import android.widget.AdapterView;
import java.io.IOException;
import android.os.Handler;

public final class FrequencyActivity extends AnalyzerActivity {

    private Button mStartButton;
    private Button mStopButton;
    private Button mShareButton;
    private LineView mLineView;
    private int mWaveformLineId = -1;
    private int mTopThresholdLineId = -1;
    private int mBottomThresholdLineId = -1;
    private int mAvgMagLineId = -1;
    private TextView mTestResultView;
    private EditText mThresholdEditText;

    private Spinner mOutputSignalSpinner;
    private FrequencySettingView mFrequencySetting;
    private TextView mBalanceTextView;
    private android.widget.SeekBar mBalanceSeekBar;
    private StreamConfigurationView mInputConfigView;
    private StreamConfigurationView mOutputConfigView;

    private static final int WAVEFORM_UPDATE_MS = 500;

    private static final float gridLinesYDbfs[] = {-150, -60, 0, 20};

    private float[] mWaveformBuffer;
    private Handler mHandler = new Handler();
    private boolean mIsUpdaterRunning = false;

    private FrequencyAnalyzer mFrequencyAnalyzer = new FrequencyAnalyzer();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mShareButton = (Button) findViewById(R.id.button_share);
        mTestResultView = (TextView) findViewById(R.id.testResultView);
        mThresholdEditText = (EditText) findViewById(R.id.thresholdEditText);

        mLineView = findViewById(R.id.line_view);
        mLineView.setUnits("Hz", "dBFS");
        mLineView.setYRange(gridLinesYDbfs[0], gridLinesYDbfs[gridLinesYDbfs.length - 1]);
        mLineView.setLogScaleX(true);
        mLineView.setGridLinesY(gridLinesYDbfs);

        android.widget.CheckBox logScaleCheckbox = findViewById(R.id.checkboxLogScale);
        logScaleCheckbox.setOnCheckedChangeListener(
                new android.widget.CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(android.widget.CompoundButton buttonView,
                            boolean isChecked) {
                        mLineView.setLogScaleX(isChecked);
                    }
                });

        mStopButton.setEnabled(false);
        mShareButton.setEnabled(false);

        mOutputSignalSpinner = (Spinner) findViewById(R.id.spinnerOutputSignal);
        String[] outputSignals = {"White Noise", "Sine", "Silence"};
        android.widget.ArrayAdapter<String> outputSignalAdapter = new android.widget.ArrayAdapter<>(
                this,
                android.R.layout.simple_spinner_item, outputSignals);
        outputSignalAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mOutputSignalSpinner.setAdapter(outputSignalAdapter);
        mOutputSignalSpinner.setOnItemSelectedListener(new OutputSignalSpinnerListener());

        mBalanceTextView = (TextView) findViewById(R.id.textBalanceSlider);
        mBalanceSeekBar = (android.widget.SeekBar) findViewById(R.id.faderBalanceSlider);
        mBalanceSeekBar.setOnSeekBarChangeListener(
                new android.widget.SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(android.widget.SeekBar seekBar, int progress,
                            boolean fromUser) {
                        float balance = progress / 100.0f;
                        mAudioOutTester.setBalance(balance);
                        mBalanceTextView.setText(
                                String.format(java.util.Locale.getDefault(), "Balance: %.2f",
                                        balance));
                    }

                    @Override
                    public void onStartTrackingTouch(android.widget.SeekBar seekBar) {
                    }

                    @Override
                    public void onStopTrackingTouch(android.widget.SeekBar seekBar) {
                    }
                });

        for (TestAudioActivity.StreamContext context : mStreamContexts) {
            if (context.isInput() && context.configurationView != null) {
                mInputConfigView = context.configurationView;
                mInputConfigView.hideSettingsView();
            } else if (!context.isInput() && context.configurationView != null) {
                mOutputConfigView = context.configurationView;
                mOutputConfigView.hideSettingsView();
            }
        }

        mFrequencySetting = findViewById(R.id.frequency_setting);
        mFrequencySetting.initialize(FrequencyPresetRepository.GROUP_FREQUENCY,
                new FrequencySettingView.OnSettingChangedListener() {
                    @Override
                    public void onSettingChanged() {
                        FrequencyPreset active = mFrequencySetting.getActivePreset();
                        if (active != null) {
                            if (mInputConfigView != null) {
                                mInputConfigView.setInputPreset(active.inputPreset);
                                if (active.preferredInput != AudioDeviceInfo.TYPE_UNKNOWN) {
                                    AudioDeviceInfo device = findInputDeviceByType(
                                            active.preferredInput);
                                    if (device != null) {
                                        mInputConfigView.setDeviceById(device.getId());
                                    } else {
                                        mInputConfigView.setDeviceById(0); // Auto-select
                                    }
                                } else {
                                    mInputConfigView.setDeviceById(0); // Auto-select
                                }
                            }

                            mOutputSignalSpinner.setSelection(
                                    FrequencySettingView.getSignalIndexForSource(
                                            active.sourceResId));
                            mBalanceSeekBar.setProgress((int) (active.balance * 100));
                            mThresholdEditText.setText(String.valueOf(active.passThreshold));
                        }
                    }
                });
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
        FrequencyPreset active = mFrequencySetting.getActivePreset();
        if (active != null) {
            // Check preferred input device
            int preferredInput = active.preferredInput;
            if (preferredInput != AudioDeviceInfo.TYPE_UNKNOWN) {
                AudioDeviceInfo device = findInputDeviceByType(preferredInput);
                if (device == null) {
                    showToast("WARNING: Preferred input device (" +
                            StreamConfiguration.deviceTypeToString(preferredInput) +
                            ") not found!");
                }
            }

            // Check preferred output device
            int preferredOutput = active.preferredOutput;
            if (preferredOutput != AudioDeviceInfo.TYPE_UNKNOWN) {
                int result = checkOutputDeviceSupported(preferredOutput);
                if (result == DEVICE_NOT_FOUND) {
                    showToast("WARNING: Preferred output device (" +
                            StreamConfiguration.deviceTypeToString(preferredOutput) +
                            ") not found!");
                } else if (result == DEVICE_CONFLICT_USB_PLUGGED) {
                    showToast("WARNING: USB speaker is plugged in while testing Built-in Speaker!");
                }
            }
        }

        try {
            openAudio();
            startAudio();
            mStartButton.setEnabled(false);
            mStopButton.setEnabled(true);
            mShareButton.setEnabled(false);
            if (mFrequencySetting != null) {
                mFrequencySetting.setEnabled(false);
            }
            if (mOutputSignalSpinner != null) {
                mOutputSignalSpinner.setEnabled(false);
            }
            if (mThresholdEditText != null) {
                mThresholdEditText.setEnabled(false);
            }
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
        if (mFrequencySetting != null) {
            mFrequencySetting.setEnabled(true);
        }
        if (mOutputSignalSpinner != null) {
            mOutputSignalSpinner.setEnabled(true);
        }
        if (mThresholdEditText != null) {
            mThresholdEditText.setEnabled(true);
        }
        keepScreenOn(false);
    }

    private native int getWindowSize();

    private class OutputSignalSpinnerListener implements
            android.widget.AdapterView.OnItemSelectedListener {

        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            mAudioOutTester.setSignalType(pos);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mAudioOutTester.setSignalType(0);
        }
    }

    private native int getFftMagnitude(float[] waveform);

    private native int getFftFrequencies(float[] frequencies);

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

                    float passThreshold = 30.0f;
                    try {
                        passThreshold = Float.parseFloat(mThresholdEditText.getText().toString());
                    } catch (NumberFormatException e) {
                        // Use default threshold
                    }

                    FrequencyBandSpec spec =
                            mFrequencySetting != null ? mFrequencySetting.getSpec() : null;
                    FrequencyAnalyzer.AnalysisResult result = mFrequencyAnalyzer.analyze(
                            mWaveformBuffer, numSamples, frequencies, numFreqs, spec, passThreshold,
                            true);

                    float maxFreq = frequencies[numFreqs - 1];
                    float minFreq = frequencies[0];
                    if (spec != null && spec.getFrequencyAnchors() != null
                            && spec.getFrequencyAnchors().length > 0) {
                        minFreq = spec.getFrequencyAnchors()[0];
                    }
                    mLineView.setXRange(minFreq, maxFreq);

                    float[] yValues = java.util.Arrays.copyOf(mWaveformBuffer, numSamples);
                    if (mWaveformLineId == -1) {
                        @SuppressWarnings("deprecation")
                        int color = getResources().getColor(R.color.waveform_line);
                        mWaveformLineId = mLineView.addLine(frequencies, yValues, color,
                                LineView.DrawMode.BAR);
                    } else {
                        mLineView.updateLine(mWaveformLineId, frequencies, yValues);
                    }

                    if (result != null) {
                        if (spec != null && spec.getFrequencyAnchors() != null) {
                            int[] anchors = spec.getFrequencyAnchors();
                            float[] cursorFreqs = new float[anchors.length];
                            for (int i = 0; i < anchors.length; i++) {
                                cursorFreqs[i] = anchors[i];
                            }
                            mLineView.setGridLinesX(cursorFreqs);
                        }

                        if (mTopThresholdLineId == -1) {
                            mTopThresholdLineId = mLineView.addLine(result.thresholdFrequencies,
                                    result.alignedTopThresholdsDbfs,
                                    android.graphics.Color.parseColor("#FFE91E63"));
                        } else {
                            mLineView.updateLine(mTopThresholdLineId, result.thresholdFrequencies,
                                    result.alignedTopThresholdsDbfs);
                        }

                        if (mBottomThresholdLineId == -1) {
                            mBottomThresholdLineId = mLineView.addLine(result.thresholdFrequencies,
                                    result.alignedBottomThresholdsDbfs,
                                    android.graphics.Color.parseColor("#FF4CAF50"));
                        } else {
                            mLineView.updateLine(mBottomThresholdLineId,
                                    result.thresholdFrequencies,
                                    result.alignedBottomThresholdsDbfs);
                        }

                        float[] avgMagX = new float[]{minFreq, maxFreq};
                        float[] avgMagY = new float[]{result.averageMagnitudeBand1,
                                result.averageMagnitudeBand1};
                        if (mAvgMagLineId == -1) {
                            mAvgMagLineId = mLineView.addLine(avgMagX, avgMagY,
                                    android.graphics.Color.parseColor("#FF2196F3"));
                        } else {
                            mLineView.updateLine(mAvgMagLineId, avgMagX, avgMagY);
                        }

                        StringBuilder sb = new StringBuilder();
                        sb.append("RESULT: ").append(result.testPassed ? "PASS" : "FAIL")
                                .append("\n");
                        sb.append("Bands: ");
                        for (int b = 0; b < result.bandEnergyPercentages.length; b++) {
                            sb.append(String.format(java.util.Locale.getDefault(), "[B%d: %.1f%%] ",
                                    b, result.bandEnergyPercentages[b]));
                        }
                        sb.append("\n");
                        sb.append(String.format(java.util.Locale.getDefault(),
                                "Avg band 1: %.2f dBFS\n", result.averageMagnitudeBand1));
                        mTestResultView.setText(sb.toString());
                        mTestResultView.setTextColor(
                                result.testPassed ? android.graphics.Color.parseColor("#FF4CAF50")
                                        : android.graphics.Color.RED);
                    } else {
                        if (mAvgMagLineId != -1) {
                            mLineView.removeLine(mAvgMagLineId);
                            mAvgMagLineId = -1;
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
