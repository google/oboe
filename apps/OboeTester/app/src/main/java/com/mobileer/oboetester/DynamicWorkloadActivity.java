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
import android.graphics.Typeface;
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
import java.util.Locale;

/**
 * Demonstrate the behavior of a changing CPU load on underruns.
 * Display the workload and the callback duration in a chart.
 * Enable PerformanceHints (ADPF).
 */
public class DynamicWorkloadActivity extends TestOutputActivityBase {
    private static final double WORKLOAD_MAX = 500.0;
    public static final double LOAD_RECOVERY_HIGH = 1.0;
    public static final double LOAD_RECOVERY_LOW = 0.95;

    private Button mStopButton;
    private Button mStartButton;
    private TextView mResultView;
    private LinearLayout mAffinityLayout;
    private ArrayList<CheckBox> mAffinityBoxes = new ArrayList<CheckBox>();
    private WorkloadUpdateThread mUpdateThread;

    private MultiLineChart mMultiLineChart;
    private MultiLineChart.Trace mMaxCpuLoadTrace;
    private MultiLineChart.Trace mWorkloadTrace;
    private CheckBox mUseAltAdpfBox;
    private CheckBox mPerfHintBox;
    private boolean mDrawChartAlways = true;
    private CheckBox mDrawAlwaysBox;
    private int mCpuCount;

    // Periodically query the status of the streams.
    protected class WorkloadUpdateThread {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 40;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;
        public static final int SNIFFER_TOGGLE_PERIOD_MSEC = 3000;
        public static final int REQUIRED_STABLE_MEASUREMENTS = 20;

        private static final double WORKLOAD_FILTER_COEFFICIENT = 0.9;
        private static final int STATE_IDLE = 0;
        private static final int STATE_BENCHMARK_TARGET = 1;
        private static final int STATE_RUN_LOW = 2;
        private static final int STATE_RUN_HIGH = 3;

        private Handler mHandler;
        private int mCount;

        private double mCpuLoadBenchmark = 0.90; // Determine workload that will hit this CPU load.
        private double mCpuLoadHigh = 0.80; // Target CPU load during HIGH cycle.

        private double mWorkloadLow = 0.0;
        private double mWorkloadHigh = 0.0;
        private double mWorkloadCurrent = 1.0;
        private double mWorkloadBenchmark = 0.0;

        private int mState = STATE_IDLE;
        private long mLastToggleTime = 0;
        private int mStableCount = 0;
        private boolean mArmLoadMonitor = false;
        private long mRecoveryTimeBegin;
        private long mRecoveryTimeEnd;
        private long mStartTimeNanos;

        String stateToString(int state) {
            switch(state) {
                case STATE_IDLE:
                    return "Idle";
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
                float cpuLoad = stream.getCpuLoad();
                float maxCpuLoad = stream.getAndResetMaxCpuLoad();
                int cpuMask = stream.getAndResetCpuMask();
                long now = System.currentTimeMillis();
                boolean drawChartOnce = false;

                switch (mState) {
                    case STATE_IDLE:
                        drawChartOnce = true; // clear old chart
                        mState = STATE_BENCHMARK_TARGET;
                        break;
                    case STATE_BENCHMARK_TARGET:
                        // prevent divide by zero
                        double targetWorkload = (mWorkloadCurrent / Math.max(cpuLoad, 0.01)) * mCpuLoadBenchmark;
                        targetWorkload = Math.min(WORKLOAD_MAX, targetWorkload);
                        // low pass filter to find matching workload
                        nextWorkload = (WORKLOAD_FILTER_COEFFICIENT * mWorkloadCurrent)
                                + ((1.0 - WORKLOAD_FILTER_COEFFICIENT) * targetWorkload);
                        if (Math.abs(cpuLoad - mCpuLoadBenchmark) < 0.04) {
                            if (++mStableCount > REQUIRED_STABLE_MEASUREMENTS) {
                                // Found the right workload.
                                mWorkloadBenchmark = nextWorkload;
                                mLastToggleTime = now;
                                mState = STATE_RUN_LOW;
                                mWorkloadLow = Math.max(1, (int)(nextWorkload * 0.02));
                                mWorkloadHigh = (int)(nextWorkload * (mCpuLoadHigh / mCpuLoadBenchmark));
                                mWorkloadTrace.setMax((float)(2.0 * nextWorkload));
                            }
                        }
                        break;
                    case STATE_RUN_LOW:
                        nextWorkload = mWorkloadLow;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_HIGH;
                            mRecoveryTimeBegin = 0;
                            mRecoveryTimeEnd = 0;
                        }
                        break;
                    case STATE_RUN_HIGH:
                        nextWorkload = mWorkloadHigh;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_LOW;
                            // Draw now when a CPU spike will not affect the result.
                            drawChartOnce = true;
                        }

                        if (mRecoveryTimeBegin == 0) {
                            if (maxCpuLoad > LOAD_RECOVERY_HIGH) {
                                mRecoveryTimeBegin = now;
                            }
                        } else if (mRecoveryTimeEnd == 0) {
                            if (maxCpuLoad < LOAD_RECOVERY_LOW) {
                                mRecoveryTimeEnd = now;
                            }
                        } else if (maxCpuLoad > LOAD_RECOVERY_LOW) {
                            mRecoveryTimeEnd = now;
                        }
                        break;
                }
                // Update chart
                float nowMicros = (System.nanoTime() - mStartTimeNanos) *  0.001f;
                mMultiLineChart.addX(nowMicros);
                mMaxCpuLoadTrace.add((float) maxCpuLoad);
                mWorkloadTrace.add((float) mWorkloadCurrent);
                if (drawChartOnce || mDrawChartAlways){
                    mMultiLineChart.update();
                }

                // Display numbers
                String recoveryTimeString = (mRecoveryTimeEnd <= mRecoveryTimeBegin) ?
                        "---" : ((mRecoveryTimeEnd - mRecoveryTimeBegin) + " msec");
                String message =
                        "#Voices: max = " + String.format(Locale.getDefault(), "%d", (int) mWorkloadBenchmark)
                        + ", now = " + String.format(Locale.getDefault(), "%d", (int) nextWorkload)
                        + "\nWorkState = " + stateToString(mState)
                        + "\nCPU = " + String.format(Locale.getDefault(), "%6.3f%c", cpuLoad * 100, '%')
                        + "\ncores = " + cpuMaskToString(cpuMask, mCpuCount)
                        + "\nRecovery = " + recoveryTimeString;
                postResult(message);
                stream.setWorkload((int)(nextWorkload));
                mWorkloadCurrent = nextWorkload;

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


    /**
     * This text will look best in a monospace font.
     * @param cpuMask CPU core bit mask
     * @return a text display of the selected cores like "--2-45-7"
     */
    // TODO move this to some utility class
    private String cpuMaskToString(int cpuMask, int cpuCount) {
        String text = "";
        long longMask = ((long) cpuMask) & 0x0FFFFFFFFL;
        int index = 0;
        while (longMask != 0 || index < cpuCount) {
            text += ((longMask & 1) != 0) ? hexDigit(index) : "-";
            longMask = longMask >> 1;
            index++;
        }
        return text;
    }

    private char hexDigit(int n) {
        byte x = (byte)(n & 0x0F);
        if (x < 10) return (char)('0' + x);
        else return (char)('A' + x);
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
        mResultView.setTypeface(Typeface.MONOSPACE);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);

        // Add a row of checkboxes for setting CPU affinity.
        mCpuCount = NativeEngine.getCpuCount();
        final int defaultCpuAffinity = 2;
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
                NativeEngine.setCpuAffinityMask(mask);
            }
        };
        mAffinityLayout = (LinearLayout)  findViewById(R.id.affinityLayout);
        for (int cpuIndex = 0; cpuIndex < mCpuCount; cpuIndex++) {
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
        mMaxCpuLoadTrace = mMultiLineChart.createTrace("CPU", Color.RED,
                0.0f, 2.0f);
        mWorkloadTrace = mMultiLineChart.createTrace("Work", Color.BLUE,
                0.0f, (float)WORKLOAD_MAX);

        // TODO remove when finished with ADPF experiments.
        mUseAltAdpfBox = (CheckBox) findViewById(R.id.use_alternative_adpf);
        mPerfHintBox = (CheckBox) findViewById(R.id.enable_perf_hint);

        mUseAltAdpfBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            setUseAlternativeAdpf(checkBox.isChecked());
        });
        mUseAltAdpfBox.setVisibility(View.GONE);

        mPerfHintBox.setOnClickListener(buttonView -> {
                CheckBox checkBox = (CheckBox) buttonView;
                setPerformanceHintEnabled(checkBox.isChecked());
                mUseAltAdpfBox.setEnabled(!checkBox.isChecked());
        });

        CheckBox hearWorkloadBox = (CheckBox) findViewById(R.id.hear_workload);
        hearWorkloadBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            setHearWorkload(checkBox.isChecked());
        });

        mDrawAlwaysBox = (CheckBox) findViewById(R.id.draw_always);
        mDrawAlwaysBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            mDrawChartAlways = checkBox.isChecked();
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
        mPerfHintBox.setEnabled(running);
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
        // Do not draw until the benchmark stage has finished.
        mDrawAlwaysBox.setChecked(false);
        mDrawChartAlways = false;
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
        onStopTest();
    }

    @Override
    public void onStopTest() {
        WorkloadUpdateThread updateThread = mUpdateThread;
        if (updateThread != null) {
            updateThread.stop();
        }
        updateButtons(false);
        super.onStopTest();
    }
}
