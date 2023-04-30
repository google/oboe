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

import android.graphics.Color;
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
    private Button mStopButton;
    private Button mStartButton;
    private TextView mResultView;
    private LinearLayout mAffinityLayout;
    private ArrayList<CheckBox> mAffinityBoxes = new ArrayList<CheckBox>();
    private WorkloadUpdateThread mUpdateThread;

    private MultiLineChart mMultiLineChart;
    private MultiLineChart.Trace mCpuLoadTrace;
    private MultiLineChart.Trace mWorkloadTrace;
    private CheckBox mUseAltAdpfBox;

    // Periodically query the status of the streams.
    protected class WorkloadUpdateThread {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 40;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;
        public static final int SNIFFER_TOGGLE_PERIOD_MSEC = 3000;
        public static final int REQUIRED_STABLE_MEASUREMENTS = 20;

        private Handler mHandler;
        private int mCount;
        private double mWorkload = 1.0;
        private double mBenchmarkCpuLoad = 0.90; // Determine workload that will hit 90% CPU load.
        private double mOperatingCpuLoad = 0.80; // CPU load during HIGH cycle.
        private double mLowWorkload = 0.0;
        private double mHighWorkload = 0.0;
        private static final double WORKLOAD_FILTER_COEFFICIENT = 0.9;
        private static final int STATE_BENCHMARK_TARGET = 0;
        private static final int STATE_RUN_LOW = 1;
        private static final int STATE_RUN_HIGH = 2;

        private int mState = STATE_BENCHMARK_TARGET;
        private long mLastToggleTime = 0;
        private int mStableCount = 0;
        private boolean mArmLoadMonitor = false;
        private long mRecoveryTimeBegin;
        private long mRecoveryTimeEnd;
        private long mStartTimeNanos;

        String stateToString(int state) {
            switch(state) {
                case STATE_BENCHMARK_TARGET:
                    return "Benchmarking";
                case STATE_RUN_LOW:
                    return "low";
                case STATE_RUN_HIGH:
                    return "HIGH";
                default:
                    return "Unrecognized";
            }
        }

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                double nextWorkload = 0.0;
                AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
                double cpuLoad = stream.getCpuLoad();
                long now = System.currentTimeMillis();

                switch (mState) {
                    case STATE_BENCHMARK_TARGET:
                        // prevent divide by zero
                        double targetWorkload = (mWorkload / Math.max(cpuLoad, 0.1)) * mBenchmarkCpuLoad;
                        // low pass filter to find matching workload
                        nextWorkload = (WORKLOAD_FILTER_COEFFICIENT * mWorkload)
                                + ((1.0 - WORKLOAD_FILTER_COEFFICIENT) * targetWorkload);
                        if (Math.abs(cpuLoad - mBenchmarkCpuLoad) < 0.04) {
                            if (++mStableCount > REQUIRED_STABLE_MEASUREMENTS) {
                                mLastToggleTime = now;
                                mState = STATE_RUN_LOW;
                                mLowWorkload = Math.max(1, (int)(nextWorkload * 0.02));
                                mHighWorkload = (int)(nextWorkload * (mOperatingCpuLoad / mBenchmarkCpuLoad));
                                mWorkloadTrace.setMax((float)(2.0 * nextWorkload));
                            }
                        }
                        break;
                    case STATE_RUN_LOW:
                        nextWorkload = mLowWorkload;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_HIGH;
                            mRecoveryTimeBegin = 0;
                            mRecoveryTimeEnd = 0;
                        }
                        break;
                    case STATE_RUN_HIGH:
                        nextWorkload = mHighWorkload;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_LOW;
                        }

                        if (mRecoveryTimeBegin == 0) {
                            if (cpuLoad > 1.0) {
                                mRecoveryTimeBegin = now;
                            }
                        } else if (mRecoveryTimeEnd == 0) {
                            if (cpuLoad < 0.90) {
                                mRecoveryTimeEnd = now;
                            }
                        } else if (cpuLoad > 0.90) {
                            mRecoveryTimeEnd = now;
                        }
                        break;
                }
                // Update chart
                float nowMicros = (System.nanoTime() - mStartTimeNanos) *  0.001f;
                mMultiLineChart.addX(nowMicros);
                mCpuLoadTrace.add((float) cpuLoad);
                mWorkloadTrace.add((float) mWorkload);
                mMultiLineChart.update();

                // Display numbers
                String recoveryTimeString = (mRecoveryTimeEnd <= mRecoveryTimeBegin) ?
                        "---" : ((mRecoveryTimeEnd - mRecoveryTimeBegin) + " msec");
                String message = "WorkState = " + stateToString(mState)
                        + "\nVoices = " + String.format("%3d", (int)nextWorkload)
                        + "\nCPU = " + String.format("%5.3f%c", cpuLoad * 100, '%')
                        + "\nRecovery = " + recoveryTimeString;
                postResult(message);
                stream.setWorkload((int)(nextWorkload));
                mWorkload = nextWorkload;

                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void start() {
            stop();
            mStartTimeNanos = System.nanoTime();
            mMultiLineChart.reset();
            mCount = 0;
            mStableCount = 0;
            mState = STATE_BENCHMARK_TARGET;
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
        final int defaultCpuAffinity = 2;
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
            if (cpuIndex == defaultCpuAffinity) {
                checkBox.setChecked(true);
            }
        }
        NativeEngine.setCpuAffinityMask(1 << defaultCpuAffinity);

        mMultiLineChart = (MultiLineChart) findViewById(R.id.multiline_chart);
        mCpuLoadTrace = mMultiLineChart.createTrace("CPU", Color.RED,  0.0f, 2.0f);
        mWorkloadTrace = mMultiLineChart.createTrace("Work", Color.BLUE, 0.0f, 100.0f);

        mUseAltAdpfBox = (CheckBox) findViewById(R.id.use_alternative_adpf);
        mUseAltAdpfBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            setUseAlternativeAdpf(checkBox.isChecked());
        });

        CheckBox perfHintBox = (CheckBox) findViewById(R.id.enable_perf_hint);
        perfHintBox.setOnClickListener(buttonView -> {
                CheckBox checkBox = (CheckBox) buttonView;
                setPerformanceHintEnabled(checkBox.isChecked());
                mUseAltAdpfBox.setEnabled(!checkBox.isChecked());
        });

        CheckBox hearWorkloadBox = (CheckBox) findViewById(R.id.hear_workload);
        hearWorkloadBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            setHearWorkload(checkBox.isChecked());
        });


        updateButtons(false);

        updateEnabledWidgets();
        hideSettingsViews(); // make more room
    }

    private void setHearWorkload(boolean checked) {
        mAudioOutTester.getCurrentAudioStream().setHearWorkload(checked);
    }

    private void setPerformanceHintEnabled(boolean checked) {
      mAudioOutTester.getCurrentAudioStream().setPerformanceHintEnabled(checked);
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
            mUpdateThread = new WorkloadUpdateThread();
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
