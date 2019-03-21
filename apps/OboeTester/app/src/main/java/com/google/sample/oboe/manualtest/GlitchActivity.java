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
import android.widget.TextView;

/**
 * Activity to measure the number of glitches.
 */
public class GlitchActivity extends AnalyzerActivity {
    private TextView mAnalyzerView;
    private Button mStartButton;
    private Button mStopButton;

    // These must match the values in LatencyAnalyzer.h
    final static int STATE_IDLE = 0;
    final static int STATE_MEASURE_NOISE =1;
    final static int STATE_IMMUNE = 2;
    final static int STATE_WAITING_FOR_SIGNAL = 3;
    final static int STATE_WAITING_FOR_LOCK = 4;
    final static int STATE_LOCKED = 5;


    // Note that these string must match the enum result_code in LatencyAnalyzer.h
    String stateToString(int resultCode) {
        switch (resultCode) {
            case STATE_IDLE:
                return "IDLE";
            case STATE_MEASURE_NOISE:
                return "MEASURE_NOISE";
            case STATE_IMMUNE:
                return "IMMUNE";
            case STATE_WAITING_FOR_SIGNAL:
                return "WAITING_FOR_SIGNAL";
            case STATE_WAITING_FOR_LOCK:
                return "WAITING_FOR_LOCK";
            case STATE_LOCKED:
                return "RUNNING";
            default:
                return "UNKNOWN";
        }
    }

    // Periodically query the status of the stream.
    protected class GlitchSniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 100;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 200;
        long timeStarted = 0;
        long timeSinceLock = 0;
        int lastGlitchCount = 0;
        int previousState = STATE_IDLE;
        boolean gotLock = false;

        private Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                int state = getAnalyzerState();
                int glitchCount = getGlitchCount();
                double signalToNoiseDB = getSignalToNoiseDB();

                boolean locked = (state == STATE_LOCKED);
                if (locked && (previousState != STATE_LOCKED)) {
                    timeSinceLock = System.currentTimeMillis();
                }
                previousState = state;
                gotLock = gotLock || locked;
                if (glitchCount > lastGlitchCount) {
                    timeSinceLock = System.currentTimeMillis();
                    lastGlitchCount = glitchCount;
                }

                long now = System.currentTimeMillis();
                double totalSeconds = (now - timeStarted) / 1000.0;

                StringBuffer message = new StringBuffer();
                message.append("state = " + state + " = " + stateToString(state) + "\n");
                message.append(String.format("signal to noise = %5.1f dB\n", signalToNoiseDB));
                message.append(String.format("Time total = %8.2f seconds\n", totalSeconds));
                double goodSeconds = (locked && (timeSinceLock > 0))
                        ? ((now - timeSinceLock) / 1000.0)
                        : 0.0;
                message.append(String.format("Time without glitches = %8.2f seconds\n",
                        goodSeconds));
                if (gotLock) {
                    message.append("glitchCount = " + glitchCount+ "\n");
                }
                setAnalyzerText(message.toString());

                // Reschedule so this task repeats
                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void startSniffer() {
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
            timeStarted = System.currentTimeMillis();
            timeSinceLock = 0;
            lastGlitchCount = 0;
            gotLock = false;
        }

        private void stopSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }
    }

    private GlitchSniffer mGlitchSniffer = new GlitchSniffer();

    native int getGlitchCount();
    native double getSignalToNoiseDB();

    private void setAnalyzerText(String s) {
        mAnalyzerView.setText(s);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_glitches);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mStopButton.setEnabled(false);
        mAnalyzerView = (TextView) findViewById(R.id.text_analyzer_result);
        updateEnabledWidgets();
        mAudioOutTester = addAudioOutputTester();

        hideSettingsViews();
    }

    @Override
    protected void onStart() {
        super.onStart();
        setActivityType(ACTIVITY_GLITCHES);
    }

    @Override
    protected void onStop() {
        mGlitchSniffer.stopSniffer();
        super.onStop();
    }

    public void onStartAudioTest(View view) {
        openAudio();
        startAudio();
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(true);
        mGlitchSniffer.startSniffer();
    }

    public void onCancel(View view) {
        stopAudioTest();
    }

    // Call on UI thread
    public void onStopAudioTest(View view) {
        stopAudioTest();
    }

    public void stopAudioTest() {
        mGlitchSniffer.stopSniffer();
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
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
