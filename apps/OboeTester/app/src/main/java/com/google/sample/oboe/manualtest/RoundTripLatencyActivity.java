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

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class RoundTripLatencyActivity extends AnalyzerActivity {

    private static final int STATE_GOT_DATA = 5; // Defined in LatencyAnalyzer.h

    private TextView mAnalyzerView;
    private Button mMeasureButton;
    private Button mCancelButton;

    // Periodically query the status of the stream.
    protected class LatencySniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                int progress = getAnalyzerProgress();
                int state = getAnalyzerState();
                String message = "progress = " + progress + ", state = " + state;
                if (state == STATE_GOT_DATA) {
                    message += "\nAnalyzing - please wait...";
                }
                setAnalyzerText(message);

                if (isAnalyzerDone()) {
                    onAnalyzerDone();
                } else {
                    // Repeat this runnable code block again.
                    mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
                }
            }
        };

        private void startSniffer() {
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }
    }

    private void onAnalyzerDone() {
        int progress = getAnalyzerProgress();
        int state = getAnalyzerState();
        int result = getMeasuredResult();
        double latencyFrames = getMeasuredLatency();
        double confidence = getMeasuredConfidence();
        double latencyMillis = latencyFrames * 1000 / getSampleRate();
        setAnalyzerText(String.format("progress = %d, state = %d\n"
                + "result = %d = %s\n"
                + "latency = %6.1f frames = %6.2f msec\nconfidence = %6.3f",
                progress,
                state,
                result, resultCodeToString(result),
                latencyFrames, latencyMillis, confidence));

        mMeasureButton.setEnabled(true);

        stopAudioTest();
    }

    private LatencySniffer mLatencySniffer = new LatencySniffer();

    native int getAnalyzerProgress();
    native double getMeasuredLatency();
    native double getMeasuredConfidence();

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
        mAnalyzerView = (TextView) findViewById(R.id.text_analyzer_result);
        updateEnabledWidgets();
        mAudioOutTester = addAudioOutputTester();

        hideSettingsViews();
    }

    @Override
    protected void onStart() {
        super.onStart();
        setActivityType(ACTIVITY_RT_LATENCY);
    }

    @Override
    protected void onStop() {
        mLatencySniffer.stopSniffer();
        super.onStop();
    }

    public void onMeasure(View view) {
        openAudio();
        startAudio();
        mLatencySniffer.startSniffer();
        mMeasureButton.setEnabled(false);
        mCancelButton.setEnabled(true);
    }

    public void onCancel(View view) {
        stopAudioTest();
    }

    // Call on UI thread
    public void stopAudioTest() {
        mLatencySniffer.stopSniffer();
        mMeasureButton.setEnabled(true);
        mCancelButton.setEnabled(false);
        stopAudio();
        closeAudio();
    }

    @Override
    boolean isOutput() {
        return false;
    }

    @Override
    public void setupEffects(int sessionId) {
    }
}
