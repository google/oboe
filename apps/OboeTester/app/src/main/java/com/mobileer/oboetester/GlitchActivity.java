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

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.IOException;

/**
 * Activity to measure the number of glitches.
 */
public class GlitchActivity extends AnalyzerActivity {
    private TextView mAnalyzerTextView;
    private Button mStartButton;
    private Button mStopButton;
    private Button mShareButton;

    // These must match the values in LatencyAnalyzer.h
    final static int STATE_IDLE = 0;
    final static int STATE_IMMUNE = 1;
    final static int STATE_WAITING_FOR_SIGNAL = 2;
    final static int STATE_WAITING_FOR_LOCK = 3;
    final static int STATE_LOCKED = 4;
    final static int STATE_GLITCHING = 5;
    String mLastGlitchReport;
    private int mInputChannel;
    private int mOutputChannel;

    native int getStateFrameCount(int state);
    native int getGlitchCount();
    native double getSignalToNoiseDB();
    native double getPeakAmplitude();

    private GlitchSniffer mGlitchSniffer;
    private NativeSniffer mNativeSniffer = createNativeSniffer();

    synchronized NativeSniffer createNativeSniffer() {
        if (mGlitchSniffer == null) {
            mGlitchSniffer = new GlitchSniffer(this);
        }
        return mGlitchSniffer;
    }

    // Note that these strings must match the enum result_code in LatencyAnalyzer.h
    String stateToString(int resultCode) {
        switch (resultCode) {
            case STATE_IDLE:
                return "IDLE";
            case STATE_IMMUNE:
                return "IMMUNE";
            case STATE_WAITING_FOR_SIGNAL:
                return "WAITING_FOR_SIGNAL";
            case STATE_WAITING_FOR_LOCK:
                return "WAITING_FOR_LOCK";
            case STATE_LOCKED:
                return "LOCKED";
            case STATE_GLITCHING:
                return "GLITCHING";
            default:
                return "UNKNOWN";
        }
    }

    // Periodically query for glitches from the native detector.
    protected class GlitchSniffer extends NativeSniffer {

        private long mTimeAtStart;
        private long mTimeOfLastGlitch;
        private double mSecondsWithoutGlitches;
        private double mMaxSecondsWithoutGlitches;
        private int mLastGlitchCount;
        private int mLastUnlockedFrames;
        private int mLastLockedFrames;
        private int mLastGlitchFrames;

        private int mStartResetCount;
        private int mLastResetCount;
        private int mPreviousState;

        private double mSignalToNoiseDB;
        private double mPeakAmplitude;

        public GlitchSniffer(Activity activity) {
            super(activity);
        }


        @Override
        public void startSniffer() {
            long now = System.currentTimeMillis();
            mTimeAtStart = now;
            mTimeOfLastGlitch = now;
            mLastUnlockedFrames = 0;
            mLastLockedFrames = 0;
            mLastGlitchFrames = 0;
            mSecondsWithoutGlitches = 0.0;
            mMaxSecondsWithoutGlitches = 0.0;
            mLastGlitchCount = 0;
            mStartResetCount = mLastResetCount;
            super.startSniffer();
        }

        public void run() {
            int state = getAnalyzerState();
            mSignalToNoiseDB = getSignalToNoiseDB();
            mPeakAmplitude = getPeakAmplitude();
            mPreviousState = state;

            long now = System.currentTimeMillis();
            int glitchCount = getGlitchCount();
            int resetCount = getResetCount();
            mLastUnlockedFrames = getStateFrameCount(STATE_WAITING_FOR_LOCK);
            int lockedFrames = getStateFrameCount(STATE_LOCKED);
            int glitchFrames = getStateFrameCount(STATE_GLITCHING);

            if (glitchFrames > mLastGlitchFrames || glitchCount > mLastGlitchCount) {
                mTimeOfLastGlitch = now;
                mSecondsWithoutGlitches = 0.0;
                onGlitchDetected();
            } else if (lockedFrames > mLastLockedFrames) {
                mSecondsWithoutGlitches = (now - mTimeOfLastGlitch) / 1000.0;
            }

            if (resetCount > mLastResetCount) {
                mLastResetCount = resetCount;
            }

            if (mSecondsWithoutGlitches > mMaxSecondsWithoutGlitches) {
                mMaxSecondsWithoutGlitches = mSecondsWithoutGlitches;
            }

            mLastGlitchCount = glitchCount;
            mLastGlitchFrames = glitchFrames;
            mLastLockedFrames = lockedFrames;
            mLastResetCount = resetCount;

            reschedule();
        }

        private String getCurrentStatusReport() {
            long now = System.currentTimeMillis();
            double totalSeconds = (now - mTimeAtStart) / 1000.0;

            StringBuffer message = new StringBuffer();
            message.append("state = " + stateToString(mPreviousState) + "\n");
            message.append(String.format("unlocked.frames = %d\n", mLastUnlockedFrames));
            message.append(String.format("locked.frames = %d\n", mLastLockedFrames));
            message.append(String.format("glitch.frames = %d\n", mLastGlitchFrames));
            message.append(String.format("reset.count = %d\n", mLastResetCount - mStartResetCount));
            message.append(String.format("peak.amplitude = %8.6f\n", mPeakAmplitude));
            if (mLastLockedFrames > 0) {
                message.append(String.format("signal.noise.ratio.db = %5.1f\n", mSignalToNoiseDB));
            }
            message.append(String.format("time.total = %8.2f seconds\n", totalSeconds));
            if (mLastLockedFrames > 0) {
                message.append(String.format("time.no.glitches = %8.2f\n", mSecondsWithoutGlitches));
                message.append(String.format("max.time.no.glitches = %8.2f\n",
                        mMaxSecondsWithoutGlitches));
                message.append(String.format("glitch.count = %d\n", mLastGlitchCount));
            }
            return message.toString();
        }

        @Override
        public String getShortReport() {
            String resultText = "#glitches = " + getLastGlitchCount()
                    + ", #resets = " + getLastResetCount()
                    + ", max no glitch = " + getMaxSecondsWithNoGlitch() + " secs\n";
            resultText += String.format("SNR = %5.1f db", mSignalToNoiseDB);
            resultText += ", #locked = " + mLastLockedFrames;
            return resultText;
        }

        @Override
        public void updateStatusText() {
            mLastGlitchReport = getCurrentStatusReport();
            setAnalyzerText(mLastGlitchReport);
        }

        public double getMaxSecondsWithNoGlitch() {
            return mMaxSecondsWithoutGlitches;
        }

        public int getLastGlitchCount() {
            return mLastGlitchCount;
        }
        public int getLastResetCount() {
            return mLastResetCount;
        }
    }

    // Called on UI thread
    protected void onGlitchDetected() {
    }

    protected void setAnalyzerText(String s) {
        mAnalyzerTextView.setText(s);
    }

    /**
     * Set tolerance to deviations from expected value.
     * The normalized value will be converted in the native code.
     * @param tolerance normalized between 0.0 and 1.0
     */
    public native void setTolerance(float tolerance);

    public void setInputChannel(int channel) {
        mInputChannel = channel;
        setInputChannelNative(channel);
    }

    public void setOutputChannel(int channel) {
        mOutputChannel = channel;
        setOutputChannelNative(channel);
    }

    public int getInputChannel() {
        return mInputChannel;
    }

    public int getOutputChannel() {
        return mOutputChannel;
    }

    public native void setInputChannelNative(int channel);

    public native void setOutputChannelNative(int channel);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mStopButton.setEnabled(false);
        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setEnabled(false);
        mAnalyzerTextView = (TextView) findViewById(R.id.text_status);
        updateEnabledWidgets();
        hideSettingsViews();
        // TODO hide sample rate menu
        StreamContext streamContext = getFirstInputStreamContext();
        if (streamContext != null) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.hideSampleRateMenu();
            }
        }
    }

    @Override
    int getActivityType() {
        return ACTIVITY_GLITCHES;
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onStop() {
        if (!isBackgroundEnabled()) {
            stopAudioTest();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (isBackgroundEnabled()) {
            stopAudioTest();
        }
        super.onDestroy();
    }

    // Called on UI thread
    public void onStartAudioTest(View view) throws IOException {
        openAudio();
        startAudioTest();
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(true);
        mShareButton.setEnabled(false);
        keepScreenOn(true);
    }

    public void startAudioTest() throws IOException {
        startAudio();
        mNativeSniffer.startSniffer();
        onTestBegan();
    }

    public void onCancel(View view) {
        stopAudioTest();
        onTestFinished();
    }

    // Called on UI thread
    public void onStopAudioTest(View view) {
        stopAudioTest();
        onTestFinished();
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mShareButton.setEnabled(false);
        keepScreenOn(false);
    }

    // Must be called on UI thread.
    public void onTestBegan() {
    }

    // Must be called on UI thread.
    public void onTestFinished() {
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mShareButton.setEnabled(true);
    }

    public void stopAudioTest() {
        mNativeSniffer.stopSniffer();
        stopAudio();
        closeAudio();
    }

    public void stopTest() {
        stopAudio();
    }

    @Override
    boolean isOutput() {
        return false;
    }

    @Override
    public void setupEffects(int sessionId) {
    }

    public double getMaxSecondsWithNoGlitch() {
        return mGlitchSniffer.getMaxSecondsWithNoGlitch();
    }

    public String getShortReport() {
        return mNativeSniffer.getShortReport();
    }

    @Override
    String getWaveTag() {
        return "glitches";
    }
}
