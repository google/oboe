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
    private FftWaveformView mWaveformView;
    private FrequencyThresholdView mWaveformViewTopThreshold;
    private FrequencyThresholdView mWaveformViewBottomThreshold;
    private TextView mTestResultView;
    private EditText mThresholdEditText;

    private Spinner mOutputSignalSpinner;
    private FrequencySetting mFrequencySetting;
    private TextView mBalanceTextView;
    private android.widget.SeekBar mBalanceSeekBar;

    private static final int WAVEFORM_UPDATE_MS = 500;
    private static final float MIN_DBFS = -150.0f;
    private static final float MAX_DBFS = 0.0f;

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

        mWaveformView = findViewById(R.id.waveform_view);
        mWaveformView.setDbfsRange(MIN_DBFS, MAX_DBFS);

        mWaveformViewTopThreshold = findViewById(R.id.waveform_view_top_threshold);
        mWaveformViewTopThreshold.setDbfsRange(MIN_DBFS, MAX_DBFS);
        mWaveformViewTopThreshold.updateTheme(
                 android.graphics.Color.parseColor("#FFE91E63"),
                 android.graphics.Color.TRANSPARENT,
                 android.graphics.Color.RED);
 
         mWaveformViewBottomThreshold = findViewById(R.id.waveform_view_bottom_threshold);
         mWaveformViewBottomThreshold.setDbfsRange(MIN_DBFS, MAX_DBFS);
         mWaveformViewBottomThreshold.updateTheme(
                 android.graphics.Color.parseColor("#FF4CAF50"),
                 android.graphics.Color.TRANSPARENT,
                 android.graphics.Color.RED);

        mStopButton.setEnabled(false);
        mShareButton.setEnabled(false);

        mOutputSignalSpinner = (Spinner) findViewById(R.id.spinnerOutputSignal);
        String[] outputSignals = {"White Noise", "Sine", "Silence"};
        android.widget.ArrayAdapter<String> outputSignalAdapter = new android.widget.ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item, outputSignals);
        outputSignalAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mOutputSignalSpinner.setAdapter(outputSignalAdapter);
        mOutputSignalSpinner.setOnItemSelectedListener(new OutputSignalSpinnerListener());

        mBalanceTextView = (TextView) findViewById(R.id.textBalanceSlider);
        mBalanceSeekBar = (android.widget.SeekBar) findViewById(R.id.faderBalanceSlider);
        mBalanceSeekBar.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(android.widget.SeekBar seekBar, int progress, boolean fromUser) {
                float balance = progress / 100.0f;
                mAudioOutTester.setBalance(balance);
                mBalanceTextView.setText(String.format(java.util.Locale.getDefault(), "Balance: %.2f", balance));
            }
            @Override
            public void onStartTrackingTouch(android.widget.SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(android.widget.SeekBar seekBar) {}
        });

        StreamConfigurationView inputConfigView = null;
        StreamConfigurationView outputConfigView = null;
        for (TestAudioActivity.StreamContext context : mStreamContexts) {
            if (context.isInput() && context.configurationView != null) {
                inputConfigView = context.configurationView;
            } else if (!context.isInput() && context.configurationView != null) {
                outputConfigView = context.configurationView;
                outputConfigView.hideSettingsView();
            }
        }
        final StreamConfigurationView finalInputConfigView = inputConfigView;
        finalInputConfigView.hideSettingsView();

        mFrequencySetting = new FrequencySetting(this,
                FrequencyPresetRepository.GROUP_FREQUENCY,
                findViewById(R.id.radioGroupBands),
                findViewById(R.id.bandSpecContainer),
                findViewById(R.id.spinnerPresets),
                new FrequencySetting.OnSettingChangedListener() {
                    @Override
                    public void onSettingChanged() {
                        FrequencyPreset active = mFrequencySetting.getActivePreset();
                        if (active != null) {
                            if (finalInputConfigView != null) {
                                finalInputConfigView.setInputPreset(active.inputPreset);
                                finalInputConfigView.setPreferredInput(active.preferredInput);
                            }

                            mOutputSignalSpinner.setSelection(FrequencySetting.getSignalIndexForSource(active.sourceResId));
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
        try {
            openAudio();
            startAudio();
            mStartButton.setEnabled(false);
            mStopButton.setEnabled(true);
            mShareButton.setEnabled(false);
            if (mFrequencySetting != null) mFrequencySetting.setEnabled(false);
            if (mOutputSignalSpinner != null) mOutputSignalSpinner.setEnabled(false);
            if (mThresholdEditText != null) mThresholdEditText.setEnabled(false);
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
        if (mFrequencySetting != null) mFrequencySetting.setEnabled(true);
        if (mOutputSignalSpinner != null) mOutputSignalSpinner.setEnabled(true);
        if (mThresholdEditText != null) mThresholdEditText.setEnabled(true);
        keepScreenOn(false);
    }

    private native int getWindowSize();
    private class OutputSignalSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
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

                    FrequencyBandSpec spec = mFrequencySetting != null ? mFrequencySetting.getSpec() : null;
                    FrequencyAnalyzer.AnalysisResult result = mFrequencyAnalyzer.analyze(
                            mWaveformBuffer, numSamples, frequencies, numFreqs, spec, passThreshold, true);
                    
                    if (numFreqs > 0) {
                        mWaveformViewTopThreshold.setMaxFrequency(frequencies[numFreqs - 1]);
                        mWaveformViewBottomThreshold.setMaxFrequency(frequencies[numFreqs - 1]);
                    }

                    if (result != null) {
                        mWaveformViewTopThreshold.setFrequencies(result.thresholdFrequencies);
                        mWaveformViewBottomThreshold.setFrequencies(result.thresholdFrequencies);

                        mWaveformViewTopThreshold.setSampleData(result.alignedTopThresholdsDbfs);
                        mWaveformViewTopThreshold.postInvalidate();
                        mWaveformViewBottomThreshold.setSampleData(result.alignedBottomThresholdsDbfs);
                        mWaveformViewBottomThreshold.postInvalidate();

                        mWaveformViewTopThreshold.setAverageMagnitude(result.averageMagnitudeBand1);

                        StringBuilder sb = new StringBuilder();
                        sb.append("RESULT: ").append(result.testPassed ? "PASS" : "FAIL").append("\n");
                        sb.append("Bands: ");
                        for (int b = 0; b < result.bandEnergyPercentages.length; b++) {
                            sb.append(String.format(java.util.Locale.getDefault(), "[B%d: %.1f%%] ", b, result.bandEnergyPercentages[b]));
                        }
                        sb.append("\n");
                        sb.append(String.format(java.util.Locale.getDefault(), "Avg band 1: %.2f dBFS\n", result.averageMagnitudeBand1));
                        mTestResultView.setText(sb.toString());
                        mTestResultView.setTextColor(result.testPassed ? android.graphics.Color.parseColor("#FF4CAF50") : android.graphics.Color.RED);
                    } else {
                        mWaveformViewTopThreshold.clearAverageMagnitude();
                    }

                    mWaveformView.setSampleData(mWaveformBuffer, 0, numSamples);
                    mWaveformView.postInvalidate();
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
