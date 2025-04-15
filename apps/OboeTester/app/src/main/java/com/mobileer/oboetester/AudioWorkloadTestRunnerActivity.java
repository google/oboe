/*
 * Copyright 2025 The Android Open Source Project
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
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Audio Workload Test Runner Activity
 */
public class AudioWorkloadTestRunnerActivity extends AppCompatActivity {

    private ExponentialSliderView mNumCallbacksSlider;
    private ExponentialSliderView mBufferSizeInBurstsSlider;
    private ExponentialSliderView mNumVoicesSlider;
    private ExponentialSliderView mAlternateNumVoicesSlider;
    private ExponentialSliderView mAlternatingPeriodMsSlider;

    private CheckBox mEnableAdpfBox;
    private CheckBox mUseSineWaveBox;

    private Button mStartButton;
    private Button mStopButton;
    private TextView mStatusTextView;
    private TextView mResultTextView;

    private Handler mHandler;
    private static final int STATUS_UPDATE_PERIOD_MS = 100;

    private Runnable mUpdateStatusRunnable = new Runnable() {
        @Override
        public void run() {
            if (!pollIsDone()) {
                mStatusTextView.setText(getStatus());
                mHandler.postDelayed(this, STATUS_UPDATE_PERIOD_MS);
            } else {
                mStatusTextView.setText(getStatus());
                int result = getResult();
                String resultText = getResultText();
                if (result == 1) {
                    mResultTextView.setText("Result: PASS\n" + resultText);
                } else if (result == -1) {
                    mResultTextView.setText("Result: FAIL\n" + resultText);
                } else {
                    mResultTextView.setText("Result: UNKNOWN\n" + resultText);
                }
                mStartButton.setEnabled(true);
                mStopButton.setEnabled(false);
                enableParamsUI(true);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_audio_workload_test_runner);

        mNumCallbacksSlider = findViewById(R.id.num_callbacks);
        mBufferSizeInBurstsSlider = findViewById(R.id.buffer_size_in_bursts);
        mNumVoicesSlider = findViewById(R.id.num_voices);
        mAlternateNumVoicesSlider = findViewById(R.id.alternate_num_voices);
        mAlternatingPeriodMsSlider = findViewById(R.id.alternating_period_ms);

        mEnableAdpfBox = findViewById(R.id.enable_adpf);
        mUseSineWaveBox = findViewById(R.id.use_sine_wave);

        mStartButton = findViewById(R.id.button_start_test);
        mStopButton = findViewById(R.id.button_stop_test);
        mStatusTextView = findViewById(R.id.status_text_view);
        mResultTextView = findViewById(R.id.result_text_view);

        mHandler = new Handler(Looper.getMainLooper());

        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        enableParamsUI(true);
    }

    public void startTest(View view) {
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(true);
        mResultTextView.setText("");
        enableParamsUI(false);

        int numCallbacks = mNumCallbacksSlider.getValue();
        int bufferSizeInBursts = mBufferSizeInBurstsSlider.getValue();
        int numVoices = mNumVoicesSlider.getValue();
        int highNumVoices = mAlternateNumVoicesSlider.getValue();
        int highLowPeriodMillis = mAlternatingPeriodMsSlider.getValue();
        boolean adpfEnabled = mEnableAdpfBox.isChecked();
        boolean sineEnabled = mUseSineWaveBox.isChecked();

        start(numCallbacks, bufferSizeInBursts, numVoices, highNumVoices, highLowPeriodMillis, adpfEnabled, sineEnabled);
        mHandler.postDelayed(mUpdateStatusRunnable, STATUS_UPDATE_PERIOD_MS);
    }

    public void stopTest(View view) {
        stop();
        mHandler.removeCallbacks(mUpdateStatusRunnable);
        int result = getResult();
        String resultText = getResultText();
        if (result == 1) {
            mResultTextView.setText("Result: PASS\n" + resultText);
        } else if (result == -1) {
            mResultTextView.setText("Result: FAIL\n" + resultText);
        } else {
            mResultTextView.setText("Result: UNKNOWN\n" + resultText);
        }
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        enableParamsUI(true);
    }

    public void enableParamsUI(boolean enabled) {
        mNumCallbacksSlider.setEnabled(enabled);
        mBufferSizeInBurstsSlider.setEnabled(enabled);
        mNumVoicesSlider.setEnabled(enabled);
        mAlternateNumVoicesSlider.setEnabled(enabled);
        mAlternatingPeriodMsSlider.setEnabled(enabled);
        mEnableAdpfBox.setEnabled(enabled);
        mUseSineWaveBox.setEnabled(enabled);
    }

    public native int start(int numCallbacks, int bufferSizeInBursts, int numVoices,
                            int highNumVoices, int highLowPeriodMillis, boolean adpfEnabled,
                            boolean sineEnabled);
    public native boolean pollIsDone();
    public native String getStatus();
    public native int stop();
    public native int getResult();
    public native String getResultText();

    static {
        System.loadLibrary("oboetester"); // Replace with your library name if different
    }
}
