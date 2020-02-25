/*
 * Copyright 2018 The Android Open Source Project
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

package com.google.sample.oboe.manualtest;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.IOException;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class RoundTripLatencyActivity extends AnalyzerActivity {

    private static final int STATE_GOT_DATA = 2; // Defined in LatencyAnalyzer.h
    private final static String LATENCY_FORMAT = "%4.2f";
    private final static String CONFIDENCE_FORMAT = "%5.3f";

    private TextView mAnalyzerView;
    private Button   mMeasureButton;
    private Button   mAverageButton;
    private Button   mCancelButton;
    private Button   mShareButton;
    private boolean  mHasRecording = false;

    private boolean mTestRunningByIntent;
    private Bundle  mBundleFromIntent;
    private int     mBufferBursts = -1;
    private Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread

    // Run the test several times and report the acverage latency.
    protected class LatencyAverager {
        private final static int AVERAGE_TEST_DELAY_MSEC = 1000; // arbitrary
        private static final int AVERAGE_MAX_ITERATIONS = 10; // arbitrary
        private int mCount = 0;

        private double  mWeightedLatencySum;
        private double  mLatencyMin;
        private double  mLatencyMax;
        private double  mConfidenceSum;
        private boolean mActive;
        private String  mLastReport = "";

        // Called on UI thread.
        String onAnalyserDone() {
            String message;
            if (!mActive) {
                message = "";
            } else if (getMeasuredResult() != 0) {
                cancel();
                updateButtons(false);
                message = "averaging cancelled due to error\n";
            } else {
                mCount++;
                double latency = getMeasuredLatencyMillis();
                double confidence = getMeasuredConfidence();
                mWeightedLatencySum += latency * confidence; // weighted average based on confidence
                mConfidenceSum += confidence;
                mLatencyMin = Math.min(mLatencyMin, latency);
                mLatencyMax = Math.max(mLatencyMax, latency);
                if (mCount < AVERAGE_MAX_ITERATIONS) {
                    mHandler.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            measureSingleLatency();
                        }
                    }, AVERAGE_TEST_DELAY_MSEC);
                } else {
                    mActive = false;
                    updateButtons(false);
                }
                message = reportAverage();
            }
            return message;
        }

        private String reportAverage() {
            String message;
            if (mCount == 0 || mConfidenceSum == 0.0) {
                message = "num.iterations = " + mCount + "\n";
            } else {
                // When I use 5.3g I only get one digit after the decimal point!
                final double averageLatency = mWeightedLatencySum / mConfidenceSum;
                final double mAverageConfidence = mConfidenceSum / mCount;
                message =
                        "average.latency.msec = " + String.format(LATENCY_FORMAT, averageLatency) + "\n"
                        + "average.confidence = " + String.format(CONFIDENCE_FORMAT, mAverageConfidence) + "\n"
                        + "min.latency.msec = " + String.format(LATENCY_FORMAT, mLatencyMin) + "\n"
                        + "max.latency.msec = " + String.format(LATENCY_FORMAT, mLatencyMax) + "\n"
                        + "num.iterations = " + mCount + "\n";
            }
            mLastReport = message;
            return message;
        }

        // Called on UI thread.
        public void start() {
            mWeightedLatencySum = 0.0;
            mConfidenceSum = 0.0;
            mLatencyMax = Double.MIN_VALUE;
            mLatencyMin = Double.MAX_VALUE;
            mCount = 0;
            mActive = true;
            mLastReport = "";
            measureSingleLatency();
        }

        public void clear() {
            mActive = false;
            mLastReport = "";
        }

        public void cancel() {
            mActive = false;
        }

        public boolean isActive() {
            return mActive;
        }

        public String getLastReport() {
            return mLastReport;
        }
    }
    LatencyAverager mLatencyAverager = new LatencyAverager();

    // Periodically query the status of the stream.
    protected class LatencySniffer {
        private int counter = 0;
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;


        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                String message;

                if (isAnalyzerDone()) {
                    message = onAnalyzerDone();
                    message += mLatencyAverager.onAnalyserDone();
                } else {
                    message = getProgressText();
                    message += "please wait... " + counter + "\n";
                    if (getAnalyzerState() == STATE_GOT_DATA) {
                        message += "ANALYZING\n";
                    }
                    // Repeat this runnable code block again.
                    mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
                }
                setAnalyzerText(message);
                counter++;
            }
        };

        private void startSniffer() {
            counter = 0;
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }
    }

    private String getProgressText() {
        int progress = getAnalyzerProgress();
        int state = getAnalyzerState();
        int resetCount = getResetCount();
        String message = String.format("progress = %d, state = %d, #resets = %d\n",
                progress, state, resetCount);
        message += mLatencyAverager.getLastReport();
        return message;
    }

    private String onAnalyzerDone() {
        String message = getResultString();
        if (mTestRunningByIntent) {
            String report = getCommonTestReport();
            report += message;
            maybeWriteTestResult(report);
        }
        mTestRunningByIntent = false;
        mHasRecording = true;
        stopAudioTest();
        return message;
    }

    @NonNull
    private String getResultString() {
        String message = String.format("rms.signal = %7.5f\n", getSignalRMS());
        message += String.format("rms.noise = %7.5f\n", getBackgroundRMS());
        int resetCount = getResetCount();
        message += String.format("reset.count = %d\n", resetCount);

        int result = getMeasuredResult();
        message += String.format("result = %d\n", result);
        message += String.format("result.text = %s\n", resultCodeToString(result));

        // Only report valid latencies.
        if (result == 0) {
            int latencyFrames = getMeasuredLatency();
            double latencyMillis = getMeasuredLatencyMillis();
            int bufferSize = mAudioOutTester.getCurrentAudioStream().getBufferSizeInFrames();
            int latencyEmptyFrames = latencyFrames - bufferSize;
            double latencyEmptyMillis = latencyEmptyFrames * 1000.0 / getSampleRate();
            message += String.format("latency.empty.frames = %d\n", latencyEmptyFrames);
            message += String.format("latency.empty.msec = " + LATENCY_FORMAT + "\n", latencyEmptyMillis);
            message += String.format("latency.frames = %d\n", latencyFrames);
            message += String.format("latency.msec = " + LATENCY_FORMAT + "\n", latencyMillis);
        }
        double confidence = getMeasuredConfidence();
        message += String.format("confidence = " + CONFIDENCE_FORMAT + "\n", confidence);
        return message;
    }

    private LatencySniffer mLatencySniffer = new LatencySniffer();

    native int getAnalyzerProgress();
    native int getMeasuredLatency();
    double getMeasuredLatencyMillis() {
        return getMeasuredLatency() * 1000.0 / getSampleRate();
    }
    native double getMeasuredConfidence();
    native double getBackgroundRMS();
    native double getSignalRMS();

    private void setAnalyzerText(String s) {
        mAnalyzerView.setText(s);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_rt_latency);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mMeasureButton = (Button) findViewById(R.id.button_measure);
        mAverageButton = (Button) findViewById(R.id.button_average);
        mCancelButton = (Button) findViewById(R.id.button_cancel);
        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setEnabled(false);
        mAnalyzerView = (TextView) findViewById(R.id.text_status);
        updateEnabledWidgets();

        hideSettingsViews();

        mBufferSizeView.setFaderNormalizedProgress(0.0); // for lowest latency

        mBundleFromIntent = getIntent().getExtras();
    }

    @Override
    public void onNewIntent(Intent intent) {
        mBundleFromIntent = intent.getExtras();
    }

    @Override
    protected void onStart() {
        super.onStart();
        setActivityType(ACTIVITY_RT_LATENCY);
        mHasRecording = false;
        updateButtons(false);
    }

    private void processBundleFromIntent() {
        if (mBundleFromIntent == null) {
            return;
        }
        if (mTestRunningByIntent) {
            return;
        }

        mResultFileName = null;
        if (mBundleFromIntent.containsKey(KEY_FILE_NAME)) {
            mTestRunningByIntent = true;
            mResultFileName = mBundleFromIntent.getString(KEY_FILE_NAME);
            getFirstInputStreamContext().configurationView.setExclusiveMode(true);
            getFirstOutputStreamContext().configurationView.setExclusiveMode(true);
            mBufferBursts = mBundleFromIntent.getInt(KEY_BUFFER_BURSTS, mBufferBursts);

            // Delay the test start to avoid race conditions.
            Handler handler = new Handler(Looper.getMainLooper()); // UI thread
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    onMeasure(null);
                }
            }, 500); // TODO where is the race, close->open?

        }
        mBundleFromIntent = null;
    }

    @Override
    public void onResume(){
        super.onResume();
        processBundleFromIntent();
    }

    @Override
    protected void onStop() {
        mLatencySniffer.stopSniffer();
        super.onStop();
    }

    public void onMeasure(View view) {
        mLatencyAverager.clear();
        measureSingleLatency();
    }

    void updateButtons(boolean running) {
        boolean busy = running || mLatencyAverager.isActive();
        mMeasureButton.setEnabled(!busy);
        mAverageButton.setEnabled(!busy);
        mCancelButton.setEnabled(running);
        mShareButton.setEnabled(!busy && mHasRecording);
    }

    private void measureSingleLatency() {
        try {
            openAudio();
            if (mBufferBursts >= 0) {
                AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
                int framesPerBurst = stream.getFramesPerBurst();
                stream.setBufferSizeInFrames(framesPerBurst * mBufferBursts);
                // override buffer size fader
                mBufferSizeView.setEnabled(false);
                mBufferBursts = -1;
            }
            startAudio();
            mLatencySniffer.startSniffer();
            updateButtons(true);
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onAverage(View view) {
        mLatencyAverager.start();
    }

    public void onCancel(View view) {
        mLatencyAverager.cancel();
        stopAudioTest();
    }

    // Call on UI thread
    public void stopAudioTest() {
        mLatencySniffer.stopSniffer();
        stopAudio();
        closeAudio();
        updateButtons(false);
    }

    @Override
    String getWaveTag() {
        return "rtlatency";
    }

    @Override
    boolean isOutput() {
        return false;
    }

    @Override
    public void setupEffects(int sessionId) {
    }
}
