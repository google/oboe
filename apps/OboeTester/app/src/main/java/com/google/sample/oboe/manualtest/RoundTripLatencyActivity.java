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
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class RoundTripLatencyActivity extends AnalyzerActivity {

    private static final int STATE_GOT_DATA = 3; // Defined in LatencyAnalyzer.h

    private TextView mAnalyzerView;
    private Button mMeasureButton;
    private Button mCancelButton;
    private Button mShareButton;

    private boolean mTestRunningByIntent;
    private Bundle mBundleFromIntent;
    private int    mBufferBursts = -1;

    // Periodically query the status of the stream.
    protected class LatencySniffer {
        private int counter = 0;
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                String message;

                if (isAnalyzerDone()) {
                    message = onAnalyzerDone();
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
        return String.format("progress = %d, state = %d, #resets = %d\n",
                progress, state, resetCount);
    }

    private String onAnalyzerDone() {
        String message = getResultString();
        mMeasureButton.setEnabled(true);
        if (mTestRunningByIntent) {
            // Add some extra information for the remote tester.
            AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
            int framesPerBurst =stream.getFramesPerBurst();
            String report = "build.fingerprint = " + Build.FINGERPRINT + "\n";
            try {
                PackageInfo pinfo = getPackageManager().getPackageInfo(getPackageName(), 0);
                report += String.format("test.version = %s\n", pinfo.versionName);
                report += String.format("test.version.code = %d\n", pinfo.versionCode);
            } catch (PackageManager.NameNotFoundException e) {
            }
            report += String.format("burst.frames = %d\n", framesPerBurst);
            int bufferSize = stream.getBufferSizeInFrames();
            report += String.format("buffer.size.frames = %d\n", bufferSize);
            int bufferCapacity = stream.getBufferCapacityInFrames();
            report += String.format("buffer.capacity.frames = %d\n", bufferCapacity);
            int sampleRate = stream.getSampleRate();
            report += String.format("sample.rate = %d\n", sampleRate);
            report += message;

            maybeWriteTestResult(report);
        }
        mTestRunningByIntent = false;

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
            double latencyMillis = latencyFrames * 1000.0 / getSampleRate();
            int bufferSize = mAudioOutTester.getCurrentAudioStream().getBufferSizeInFrames();
            int latencyEmptyFrames = latencyFrames - bufferSize;
            double latencyEmptyMillis = latencyEmptyFrames * 1000.0 / getSampleRate();
            message += String.format("latency.empty.frames = %d\n", latencyEmptyFrames);
            message += String.format("latency.empty.msec = %6.2f\n", latencyEmptyMillis);
            message += String.format("latency.frames = %d\n", latencyFrames);
            message += String.format("latency.msec = %6.2f\n", latencyMillis);
        }
        double confidence = getMeasuredConfidence();
        message += String.format("confidence = %6.3f\n", confidence);
        return message;
    }

    private LatencySniffer mLatencySniffer = new LatencySniffer();

    native int getAnalyzerProgress();
    native int getMeasuredLatency();
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
        mCancelButton = (Button) findViewById(R.id.button_cancel);
        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setEnabled(false);
        mAnalyzerView = (TextView) findViewById(R.id.text_analyzer_result);
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
        mShareButton.setEnabled(false);
    }

    private void maybeWriteTestResult(String resultString) {
        if (mResultFileName == null) return;
        writeTestResultIfPermitted(resultString);
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
        mMeasureButton.setEnabled(false);
        mCancelButton.setEnabled(true);
        mShareButton.setEnabled(false);
    }

    public void onCancel(View view) {
        stopAudioTest();
    }

    // Call on UI thread
    public void stopAudioTest() {
        mLatencySniffer.stopSniffer();
        mMeasureButton.setEnabled(true);
        mCancelButton.setEnabled(false);
        mShareButton.setEnabled(true);
        stopAudio();
        closeAudio();
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
