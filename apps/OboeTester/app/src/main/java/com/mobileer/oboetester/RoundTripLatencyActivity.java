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

package com.mobileer.oboetester;

import static com.mobileer.oboetester.IntentBasedTestSupport.configureStreamsFromBundle;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import androidx.annotation.NonNull;

import java.io.File;
import java.io.IOException;
import java.util.Locale;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class RoundTripLatencyActivity extends AnalyzerActivity {

    // STATEs defined in LatencyAnalyzer.h
    private static final int STATE_MEASURE_BACKGROUND = 0;
    private static final int STATE_IN_PULSE = 1;
    private static final int STATE_GOT_DATA = 2;
    private final static String LATENCY_FORMAT = "%4.2f";
    // When I use 5.3g I only get one digit after the decimal point!
    private final static String CONFIDENCE_FORMAT = "%5.3f";

    private TextView mAnalyzerView;
    private Button   mMeasureButton;
    private Button   mAverageButton;
    private Button   mCancelButton;
    private Button   mShareButton;
    private boolean  mHasRecording = false;

    private int     mBufferBursts = -1;
    private Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread

    DoubleStatistics mTimestampLatencyStats = new DoubleStatistics(); // for single measurement

    // Run the test several times and report the average latency.
    protected class AverageLatencyTestRunner {
        private final static int AVERAGE_TEST_DELAY_MSEC = 1000; // arbitrary
        private static final int GOOD_RUNS_REQUIRED = 5; // arbitrary
        private static final int MAX_BAD_RUNS_ALLOWED = 5; // arbitrary
        private int mBadCount = 0; // number of bad measurements

        DoubleStatistics mLatencies = new DoubleStatistics();
        DoubleStatistics mConfidences = new DoubleStatistics();
        DoubleStatistics mTimestampLatencies = new DoubleStatistics(); // for multiple measurements
        private boolean mActive;
        private String  mLastReport = "";

        private int getGoodCount() {
            return mLatencies.count();
        }

        // Called on UI thread.
        String onAnalyserDone() {
            String message;
            boolean reschedule = false;
            if (!mActive) {
                message = "";
            } else if (getMeasuredResult() != 0) {
                mBadCount++;
                if (mBadCount > MAX_BAD_RUNS_ALLOWED) {
                    cancel();
                    updateButtons(false);
                    message = "averaging cancelled due to error\n";
                } else {
                    message = "skipping this bad run, "
                            + mBadCount + " of " + MAX_BAD_RUNS_ALLOWED + " max\n";
                    reschedule = true;
                }
            } else {
                double latency = getMeasuredLatencyMillis();
                mLatencies.add(latency);
                double confidence = getMeasuredConfidence();
                mConfidences.add(confidence);

                double timestampLatency = getTimestampLatencyMillis();
                if (timestampLatency > 0.0) {
                    mTimestampLatencies.add(timestampLatency);
                }
                if (getGoodCount() < GOOD_RUNS_REQUIRED) {
                    reschedule = true;
                } else {
                    mActive = false;
                    updateButtons(false);
                }
                message = reportAverage();
            }
            if (reschedule) {
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        measureSingleLatency();
                    }
                }, AVERAGE_TEST_DELAY_MSEC);
            }
            return message;
        }

        private String reportAverage() {
            String message;
            if (getGoodCount() == 0 || mConfidences.getSum() == 0.0) {
                message = "num.iterations = " + getGoodCount() + "\n";
            } else {
                final double mAverageConfidence = mConfidences.calculateMean();
                double meanLatency = mLatencies.calculateMean();
                double meanAbsoluteDeviation = mLatencies.calculateMeanAbsoluteDeviation(meanLatency);
                double timestampLatencyMean = -1;
                double timestampLatencyMAD = 0.0;
                if (mTimestampLatencies.count() > 0) {
                    timestampLatencyMean = mTimestampLatencies.calculateMean();
                    timestampLatencyMAD =
                            mTimestampLatencies.calculateMeanAbsoluteDeviation(timestampLatencyMean);
                }
                message = "average.latency.msec = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, meanLatency) + "\n"
                        + "mean.absolute.deviation = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, meanAbsoluteDeviation) + "\n"
                        + "average.confidence = "
                        + String.format(Locale.getDefault(), CONFIDENCE_FORMAT, mAverageConfidence) + "\n"
                        + "min.latency.msec = " + String.format(Locale.getDefault(), LATENCY_FORMAT, mLatencies.getMin()) + "\n"
                        + "max.latency.msec = " + String.format(Locale.getDefault(), LATENCY_FORMAT, mLatencies.getMax()) + "\n"
                        + "num.iterations = " + mLatencies.count() + "\n"
                        + "timestamp.latency.msec = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, timestampLatencyMean) + "\n"
                        + "timestamp.latency.mad = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, timestampLatencyMAD) + "\n";
            }
            message += "num.failed = " + mBadCount + "\n";
            message += "\n"; // mark end of average report
            mLastReport = message;
            return message;
        }

        // Called on UI thread.
        public void start() {
            mLatencies = new DoubleStatistics();
            mConfidences = new DoubleStatistics();
            mTimestampLatencies = new DoubleStatistics();
            mBadCount = 0;
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
    AverageLatencyTestRunner mAverageLatencyTestRunner = new AverageLatencyTestRunner();

    // Periodically query the status of the stream.
    protected class LatencySniffer {
        private int counter = 0;
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                double timestampLatency = -1.0;
                int state = getAnalyzerState();
                if (state == STATE_MEASURE_BACKGROUND || state == STATE_IN_PULSE) {
                    timestampLatency = measureTimestampLatency();
                    // Some configurations do not support input timestamps.
                    if (timestampLatency > 0) {
                        mTimestampLatencyStats.add(timestampLatency);
                    }
                }

                String message;
                if (isAnalyzerDone()) {
                    if (mAverageLatencyTestRunner.isActive()) {
                        message = mAverageLatencyTestRunner.onAnalyserDone();
                    } else {
                        message = getResultString();
                    }
                    File resultFile = onAnalyzerDone();
                    if (resultFile != null) {
                        message = "result.file = " + resultFile.getAbsolutePath() + "\n" + message;
                    }
                } else {
                    message = getProgressText();
                    message += "please wait... " + counter + "\n";
                    message += convertStateToString(getAnalyzerState()) + "\n";

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

    static String convertStateToString(int state) {
        switch (state) {
            case STATE_MEASURE_BACKGROUND: return "BACKGROUND";
            case STATE_IN_PULSE: return "RECORDING";
            case STATE_GOT_DATA: return "ANALYZING";
            default: return "DONE";
        }
    }

    private String getProgressText() {
        int progress = getAnalyzerProgress();
        int state = getAnalyzerState();
        int resetCount = getResetCount();
        String message = String.format(Locale.getDefault(), "progress = %d\nstate = %d\n#resets = %d\n",
                progress, state, resetCount);
        message += mAverageLatencyTestRunner.getLastReport();
        return message;
    }

    private File onAnalyzerDone() {
        File resultFile = null;
        if (mTestRunningByIntent) {
            String report = getCommonTestReport();
            report += getResultString();
            resultFile = maybeWriteTestResult(report);
        }
        mTestRunningByIntent = false;
        mHasRecording = true;
        stopAudioTest();
        return resultFile;
    }

    @NonNull
    private String getResultString() {
        int result = getMeasuredResult();
        int resetCount = getResetCount();
        double confidence = getMeasuredConfidence();
        String message = "";

        message += String.format(Locale.getDefault(), "confidence = " + CONFIDENCE_FORMAT + "\n", confidence);
        message += String.format(Locale.getDefault(), "result.text = %s\n", resultCodeToString(result));

        // Only report valid latencies.
        if (result == 0) {
            int latencyFrames = getMeasuredLatency();
            double latencyMillis = getMeasuredLatencyMillis();
            int bufferSize = mAudioOutTester.getCurrentAudioStream().getBufferSizeInFrames();
            int latencyEmptyFrames = latencyFrames - bufferSize;
            double latencyEmptyMillis = latencyEmptyFrames * 1000.0 / getSampleRate();
            message += String.format(Locale.getDefault(), "latency.msec = " + LATENCY_FORMAT + "\n", latencyMillis);
            message += String.format(Locale.getDefault(), "latency.frames = %d\n", latencyFrames);
            message += String.format(Locale.getDefault(), "latency.empty.msec = " + LATENCY_FORMAT + "\n", latencyEmptyMillis);
            message += String.format(Locale.getDefault(), "latency.empty.frames = %d\n", latencyEmptyFrames);
        }

        message += String.format(Locale.getDefault(), "rms.signal = %7.5f\n", getSignalRMS());
        message += String.format(Locale.getDefault(), "rms.noise = %7.5f\n", getBackgroundRMS());
        message += String.format(Locale.getDefault(), "correlation = " + CONFIDENCE_FORMAT + "\n",
                getMeasuredCorrelation());
        double timestampLatency = getTimestampLatencyMillis();
        message += String.format(Locale.getDefault(), "timestamp.latency.msec = " + LATENCY_FORMAT + "\n",
                timestampLatency);
        if (mTimestampLatencyStats.count() > 0) {
            message += String.format(Locale.getDefault(), "timestamp.latency.mad = " + LATENCY_FORMAT + "\n",
                    mTimestampLatencyStats.calculateMeanAbsoluteDeviation(timestampLatency));
        }
        message +=  "timestamp.latency.count = " + mTimestampLatencyStats.count() + "\n";
        message += String.format(Locale.getDefault(), "reset.count = %d\n", resetCount);
        message += String.format(Locale.getDefault(), "result = %d\n", result);

        return message;
    }

    private LatencySniffer mLatencySniffer = new LatencySniffer();

    double getMeasuredLatencyMillis() {
        return getMeasuredLatency() * 1000.0 / getSampleRate();
    }

    double getTimestampLatencyMillis() {
        if (mTimestampLatencyStats.count() == 0) return -1.0;
        else return mTimestampLatencyStats.calculateMean();
    }

    native int getAnalyzerProgress();
    native int getMeasuredLatency();
    native double measureTimestampLatency();
    native double getMeasuredConfidence();
    native double getMeasuredCorrelation();
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

        mCommunicationDeviceView = (CommunicationDeviceView) findViewById(R.id.comm_device_view);

    }

    @Override
    int getActivityType() {
        return ACTIVITY_RT_LATENCY;
    }

    @Override
    protected void onStart() {
        super.onStart();
        mHasRecording = false;
        updateButtons(false);
    }

    @Override
    public void startTestUsingBundle() {
        try {
            StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
            StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
            configureStreamsFromBundle(mBundleFromIntent, requestedInConfig, requestedOutConfig);

            mBufferBursts = mBundleFromIntent.getInt(IntentBasedTestSupport.KEY_BUFFER_BURSTS, mBufferBursts);

            onMeasure(null);
        } finally {
            mBundleFromIntent = null;
        }
    }

    @Override
    protected void onStop() {
        mLatencySniffer.stopSniffer();
        super.onStop();
    }

    public void onMeasure(View view) {
        mAverageLatencyTestRunner.clear();
        measureSingleLatency();
    }

    void updateButtons(boolean running) {
        boolean busy = running || mAverageLatencyTestRunner.isActive();
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
            mTimestampLatencyStats  = new DoubleStatistics();
            mLatencySniffer.startSniffer();
            updateButtons(true);
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onAverage(View view) {
        mAverageLatencyTestRunner.start();
    }

    public void onCancel(View view) {
        mAverageLatencyTestRunner.cancel();
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
}
