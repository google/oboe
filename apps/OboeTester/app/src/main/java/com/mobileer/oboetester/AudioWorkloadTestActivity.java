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
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.List;

/**
 * Audio Workload Test
 */
public class AudioWorkloadTestActivity extends AppCompatActivity {

    private ExponentialSliderView mNumCallbacksSlider;
    private ExponentialSliderView mBufferSizeInBurstsSlider;
    private ExponentialSliderView mNumVoicesSlider;
    private ExponentialSliderView mAlternateNumVoicesSlider;
    private ExponentialSliderView mAlternatingPeriodMsSlider;

    private CheckBox mEnableAdpfBox;
    private CheckBox mUseSineWaveBox;

    private int mCpuCount;
    private LinearLayout mAffinityLayout;
    private ArrayList<CheckBox> mAffinityBoxes = new ArrayList<CheckBox>();

    private Button mOpenButton;
    private Button mStartButton;
    private Button mStopButton;
    private Button mCloseButton;

    private TextView mStreamInfoView;
    private TextView mCurrentStatusView;
    private TextView mCallbackStatisticsTextView;

    private UpdateThread mUpdateThread;

    public static class CallbackStatus {
        public int numVoices;
        public long beginTimeNs;
        public long finishTimeNs;
        public int xRunCount;
        public int cpuIndex;

        public CallbackStatus(int numVoices, long beginTimeNs, long finishTimeNs, int xRunCount, int cpuIndex) {
            this.numVoices = numVoices;
            this.beginTimeNs = beginTimeNs;
            this.finishTimeNs = finishTimeNs;
            this.xRunCount = xRunCount;
            this.cpuIndex = cpuIndex;
        }

        @NonNull
        @Override
        public String toString() {
            return "CallbackStatus{" +
                    "numVoices=" + numVoices +
                    ", beginTime=" + beginTimeNs +
                    ", finishTime=" + finishTimeNs +
                    ", xRunCount=" + xRunCount +
                    ", cpuIndex=" + cpuIndex +
                    '}';
        }

        public String toShortString() {
            return numVoices + ", " + beginTimeNs + ", " + finishTimeNs + ", " +
                    xRunCount + ", " + cpuIndex;
        }
    }

    // Periodically query the status of the streams.
    protected class UpdateThread {
        private Handler mHandler;
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 40;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                mCurrentStatusView.setText(String.format("#%d, xRuns: %d, time: %.3fms, running: %b",
                        getCallbackCount(), getXRunCount(), getLastDurationNs() / 1000000.0f, isRunning()));
                if (isRunning()) {
                    mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
                } else {
                    stopTest();
                }
            }
        };

        private void start() {
            stop();
            mHandler = new Handler(Looper.getMainLooper());
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stop() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }

    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_audio_workload_test);

        mNumCallbacksSlider = (ExponentialSliderView) findViewById(R.id.num_callbacks);
        mBufferSizeInBurstsSlider = (ExponentialSliderView) findViewById(R.id.buffer_size_in_bursts);
        mNumVoicesSlider = (ExponentialSliderView) findViewById(R.id.num_voices);
        mAlternateNumVoicesSlider = (ExponentialSliderView) findViewById(R.id.alternate_num_voices);
        mAlternatingPeriodMsSlider = (ExponentialSliderView) findViewById(R.id.alternating_period_ms);

        mEnableAdpfBox = (CheckBox) findViewById(R.id.enable_adpf);
        mUseSineWaveBox = (CheckBox) findViewById(R.id.use_sine_wave);

        mOpenButton = (Button) findViewById(R.id.button_open);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mCloseButton = (Button) findViewById(R.id.button_close);

        mStreamInfoView = (TextView) findViewById(R.id.stream_info_view);
        mCurrentStatusView = (TextView) findViewById(R.id.current_status_view);
        mCallbackStatisticsTextView = (TextView) findViewById(R.id.callback_statistics_text_view);

        mCpuCount = getCpuCount();
        final int defaultCpuAffinityMask = 0;
        View.OnClickListener checkBoxListener = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                // Create a mack from all the checkboxes.
                int mask = 0;
                for (int cpuIndex = 0; cpuIndex < mCpuCount; cpuIndex++) {
                    CheckBox checkBox = mAffinityBoxes.get(cpuIndex);
                    if (checkBox.isChecked()) {
                        mask |= (1 << cpuIndex);
                    }
                }
                setCpuAffinityForCallback(mask);
            }
        };
        mAffinityLayout = (LinearLayout)  findViewById(R.id.affinity_layout);
        for (int cpuIndex = 0; cpuIndex < mCpuCount; cpuIndex++) {
            CheckBox checkBox = new CheckBox(AudioWorkloadTestActivity.this);
            mAffinityLayout.addView(checkBox);
            mAffinityBoxes.add(checkBox);
            checkBox.setText(cpuIndex + "");
            checkBox.setOnClickListener(checkBoxListener);
            if (((1 << cpuIndex) & defaultCpuAffinityMask) != 0) {
                checkBox.setChecked(true);
            }
        }
        setCpuAffinityForCallback(defaultCpuAffinityMask);

        mOpenButton.setEnabled(true);
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(false);
        mCloseButton.setEnabled(false);
    }

    public void openAudio(View view) {
        open();
        mStreamInfoView.setText(String.format("burst: %d, sr: %d, buffer: %d", getFramesPerBurst(),
                getSampleRate(), getBufferSizeInFrames()));

        mOpenButton.setEnabled(false);
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mCloseButton.setEnabled(false);
    }

    public void startTest(View view) {
        start(mNumCallbacksSlider.getValue(), mBufferSizeInBurstsSlider.getValue(),
                mNumVoicesSlider.getValue(), mAlternateNumVoicesSlider.getValue(),
                mAlternatingPeriodMsSlider.getValue(), mEnableAdpfBox.isChecked(),
                mUseSineWaveBox.isChecked());

        mOpenButton.setEnabled(false);
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(true);
        mCloseButton.setEnabled(false);

        enableParamsUI(false);

        mUpdateThread = new UpdateThread();
        mUpdateThread.start();
    }

    public void stopTest(View view) {
        stopTest();
    }

    private void stopTest() {
        stop();
        StringBuilder callbackStatisticsText = new StringBuilder();
        List<CallbackStatus> callbackStatuses = getCallbackStatistics();
        int index = 0;
        for (CallbackStatus callbackStatus : callbackStatuses) {
            index++;
            callbackStatisticsText.append(String.format("%d, %d, %.3f, %d, %d\n",
                    index, callbackStatus.numVoices, (callbackStatus.finishTimeNs - callbackStatus.beginTimeNs) / 1000000.0f, callbackStatus.xRunCount, callbackStatus.cpuIndex));
        }
        mCallbackStatisticsTextView.setText(callbackStatisticsText);

        if (mUpdateThread != null) {
            mUpdateThread.stop();
        }

        mOpenButton.setEnabled(false);
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mCloseButton.setEnabled(true);

        enableParamsUI(true);
    }

    public void closeAudio(View view) {
        close();

        mOpenButton.setEnabled(true);
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(false);
        mCloseButton.setEnabled(false);
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

    private native int open();
    private native int getFramesPerBurst();
    private native int getSampleRate();
    private native int getBufferSizeInFrames();
    private native int start(int numCallbacks, int bufferSizeInBursts, int numVoices,
                             int numAlternateVoices, int alternatingPeriodMs, boolean adpfEnabled,
                             boolean sineEnabled);
    private native int getCpuCount();
    private native int setCpuAffinityForCallback(int mask);
    private native int getXRunCount();
    private native int getCallbackCount();
    private native long getLastDurationNs();
    private native boolean isRunning();
    private native int stop();
    private native int close();
    private native List<CallbackStatus> getCallbackStatistics();
}
