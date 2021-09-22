/*
 * Copyright 2015 The Android Open Source Project
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

import java.io.IOException;
import java.util.ArrayList;

public class DynamicWorkloadActivity extends TestOutputActivityBase {
    public static final int INT = 100;
    private Button mStopButton;
    private Button mStartButton;
    private TextView mResultView;
    private LinearLayout mAffinityLayout;
    private ArrayList<CheckBox> mAffinityBoxes = new ArrayList<CheckBox>();
    private WorkloadUpdateThread mUpdateThread = new WorkloadUpdateThread();

    // Periodically query the status of the streams.
    protected class WorkloadUpdateThread {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;
        public static final int SNIFFER_TOGGLE_PERIOD_MSEC = 3000;
        public static final int REQUIRED_STABLE_MEASUREMENTS = 30;

        private Handler mHandler;
        private int mCount;
        private double mWorkload = 1.0;
        private double mTargetCpuLoad = 0.80;
        private double mLowWorkload = 0.0;
        private double mHighWorkload = 0.0;
        private static final int STATE_MEASURE_TARGET = 0;
        private static final int STATE_RUN_LOW = 1;
        private static final int STATE_RUN_HIGH = 2;
        private int mState = STATE_MEASURE_TARGET;
        private long mLastToggleTime = 0;
        private int mStableCount = 0;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                double nextWorkload = 0.0;
                AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
                double cpuLoad = stream.getCpuLoad();
                cpuLoad = Math.max(cpuLoad, 0.01); // prevent divide by zero
                long now = System.currentTimeMillis();

                switch (mState) {
                    case STATE_MEASURE_TARGET:
                        double targetWorkload = (mWorkload / cpuLoad) * mTargetCpuLoad;
                        // low pass filter to find matching workload
                        nextWorkload = ((4 * mWorkload) + targetWorkload) / 5;
                        if (Math.abs(cpuLoad - mTargetCpuLoad) < 0.04) {
                            if (mStableCount++ > REQUIRED_STABLE_MEASUREMENTS) {
                                mLastToggleTime = now;
                                mState = STATE_RUN_LOW;
                                mLowWorkload = nextWorkload * 0.02;
                                mHighWorkload = nextWorkload;
                            }
                        }
                        break;
                    case STATE_RUN_LOW:
                        nextWorkload = mLowWorkload;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_HIGH;
                        }
                        break;
                    case STATE_RUN_HIGH:
                        nextWorkload = mHighWorkload;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_LOW;
                        }
                        break;
                }
                String message = "Count = " + mCount++
                        + "\nmState = " + mState
                        + "\nCPU = " + String.format("%5.3f%c", cpuLoad * 100, '%')
                        + "\nWork = " + String.format("%5.3f", nextWorkload);
                postResult(message);
                stream.setWorkload(nextWorkload);
                mWorkload = nextWorkload;

                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void start() {
            stop();
            mCount = 0;
            mStableCount = 0;
            mState = STATE_MEASURE_TARGET;
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
    protected void inflateActivity() {
        setContentView(R.layout.activity_dynamic_workload);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAudioOutTester = addAudioOutputTester();

        mResultView = (TextView) findViewById(R.id.resultView);

        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);

        // Add a row of checkboxes for setting CPU affinity.
        final int cpuCount = NativeEngine.getCpuCount();
        View.OnClickListener checkBoxListener = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                // Create a mack from all the checkboxes.
                int mask = 0;
                for (int cpuIndex = 0; cpuIndex < cpuCount; cpuIndex++) {
                    CheckBox checkBox = mAffinityBoxes.get(cpuIndex);
                    if (checkBox.isChecked()) {
                        mask |= (1 << cpuIndex);
                    }
                }
                NativeEngine.setCpuAffinityMask(mask);
            }
        };
        mAffinityLayout = (LinearLayout)  findViewById(R.id.affinityLayout);
        for (int cpuIndex = 0; cpuIndex < cpuCount; cpuIndex++) {
            CheckBox checkBox = new CheckBox(DynamicWorkloadActivity.this);
            mAffinityLayout.addView(checkBox);
            mAffinityBoxes.add(checkBox);
            checkBox.setText(cpuIndex + "");
            checkBox.setOnClickListener(checkBoxListener);
        }

        updateButtons(false);

        updateEnabledWidgets();

    }

    private void updateButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
    }

    private void postResult(final String text) {
        runOnUiThread(new Runnable() {
            public void run() {
                mResultView.setText(text);
            }
        });
    }

    @Override
    int getActivityType() {
        return ACTIVITY_DYNAMIC_WORKLOAD;
    }

    public void startTest(View view) {
        try {
            openAudio();
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Open audio failed!");
            return;
        }
        try {
            super.startAudio();
            updateButtons(true);
            postResult("Running test");
            mUpdateThread.start();
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Start audio failed! " + e.getMessage());
            return;
        }
    }

    public void stopTest(View view) {
        mUpdateThread.stop();
        stopAudio();
        closeAudio();
        updateButtons(false);
    }
}
