/*
 * Copyright 2017 The Android Open Source Project
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

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.Toast;

import com.google.sample.oboe.manualtest.R;

import java.io.IOException;

/**
 * Base class for other Activities.
 */
abstract class TestAudioActivity extends Activity {
    public static final String TAG = "TestOboe";

    protected static final int FADER_THRESHOLD_MAX = 1000;
    public static final int STATE_OPEN = 0;
    public static final int STATE_STARTED = 1;
    public static final int STATE_PAUSED = 2;
    public static final int STATE_STOPPED = 3;
    public static final int STATE_CLOSED = 4;
    public static final int COLOR_ACTIVE = 0xFFD0D0A0;
    public static final int COLOR_IDLE = 0xFFD0D0D0;

    private int mState = STATE_CLOSED;
    protected TextView mStatusView;
    protected String audioManagerSampleRate;
    protected int audioManagerFramesPerBurst;
    protected AudioStreamTester mAudioStreamTester;
    protected StreamConfigurationView mStreamConfigurationView;
    private Button mOpenButton;
    private Button mStartButton;
    private Button mPauseButton;
    private Button mStopButton;
    private Button mCloseButton;
    private MyStreamSniffer mStreamSniffer;
    private CheckBox mCallbackReturnStopBox;

    // Periodically query the status of the stream.
    protected class MyStreamSniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private int mFramesPerBurst = 1;
        private int mNumUpdates = 0;
        private Handler mHandler;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                // Handler runs this on the main UI thread.
                AudioStreamBase.StreamStatus status = mAudioStreamTester.getCurrentAudioStream().getStreamStatus();
                updateStreamStatusView(status);
                // Repeat this runnable code block again.
                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void startStreamSniffer() {
            stopStreamSniffer();
            mNumUpdates = 0;
            mHandler = new Handler(Looper.getMainLooper());
            // Start the initial runnable task by posting through the handler
            mFramesPerBurst = mAudioStreamTester.getCurrentAudioStream().getFramesPerBurst();
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopStreamSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }

        // These are constantly changing.
        private void updateStreamStatusView(final AudioStreamBase.StreamStatus status) {
            if (status.bufferSize < 0 || status.framesWritten < 0) {
                return;
            }
            int numBuffers = 0;
            if (status.bufferSize > 0 && mFramesPerBurst > 0) {
                numBuffers = status.bufferSize / mFramesPerBurst;
            }
            String latencyText = (status.latency < 0.0)
                    ? "?"
                    : String.format("%6.1f msec", status.latency);
            final String msg = "buffer size = "
                    + ((status.bufferSize < 0) ? "?" : status.bufferSize) + " = "
                    + numBuffers + " * " + mFramesPerBurst + ", xRunCount = "
                    +  ((status.xRunCount < 0) ? "?" : status.xRunCount) + "\n"
                    + "frames written " + status.framesWritten + " - read " + status.framesRead
                    + " = " + (status.framesWritten - status.framesRead) + "\n"

                    + "# " + mNumUpdates++
                    + ", latency = " + latencyText
                    + ", state = " + status.state
                    + ", #callbacks " + status.callbackCount
                    ;
            runOnUiThread(new Runnable() {
                public void run() {
                    mStatusView.setText(msg);
                    updateStreamDisplay();
                }
            });
        }
    }

    void updateStreamDisplay() {
    }

    @Override
    protected void onDestroy() {
        try {
            mAudioStreamTester.stop();
        } catch (IOException e) {
            e.printStackTrace();
        }
        mState = STATE_CLOSED;
        super.onDestroy();
    }

    int getState() {
        return mState;
    }

    protected void updateEnabledWidgets() {
        if (mOpenButton != null) {
            mOpenButton.setBackgroundColor(mState == STATE_OPEN ? COLOR_ACTIVE : COLOR_IDLE);
            mStartButton.setBackgroundColor(mState == STATE_STARTED ? COLOR_ACTIVE : COLOR_IDLE);
            mPauseButton.setBackgroundColor(mState == STATE_PAUSED ? COLOR_ACTIVE : COLOR_IDLE);
            mStopButton.setBackgroundColor(mState == STATE_STOPPED ? COLOR_ACTIVE : COLOR_IDLE);
            mCloseButton.setBackgroundColor(mState == STATE_CLOSED ? COLOR_ACTIVE : COLOR_IDLE);
        }
        mStreamConfigurationView.setChildrenEnabled(mState == STATE_CLOSED);
    }

    abstract boolean isOutput();

    protected void findAudioCommon() {
        mStatusView = (TextView) findViewById(R.id.statusView);

        mOpenButton = (Button) findViewById(R.id.button_open);
        if (mOpenButton != null) {
            mStartButton = (Button) findViewById(R.id.button_start);
            mPauseButton = (Button) findViewById(R.id.button_pause);
            mStopButton = (Button) findViewById(R.id.button_stop);
            mCloseButton = (Button) findViewById(R.id.button_close);
        }

        mStreamConfigurationView = (StreamConfigurationView)
                findViewById(R.id.outputStreamConfiguration);
        mStreamConfigurationView.setOutput(isOutput());

        queryNativeAudioParameters();

        mCallbackReturnStopBox = (CheckBox) findViewById(R.id.callbackReturnStop);
        if (mCallbackReturnStopBox != null) {
            mCallbackReturnStopBox.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    OboeAudioStream.setCallbackReturnStop(mCallbackReturnStopBox.isChecked());
                }
            });
        }
        OboeAudioStream.setCallbackReturnStop(false);

        mStreamSniffer = new MyStreamSniffer();
    }

    private void queryNativeAudioParameters() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        audioManagerSampleRate = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        String audioManagerFramesPerBurstText = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        audioManagerFramesPerBurst = Integer.parseInt(audioManagerFramesPerBurstText);
    }

    abstract public void setupEffects(int sessionId);

    protected void showToast(String message) {
        Toast.makeText(this, "Error: " + message, Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onStop() {
        Log.i(TAG, "onStop() called so stopping audio =========================");
        stopAudio();
        closeAudio();
        super.onStop();
    }

    public void openAudio(View view) {
        openAudio();
    }

    public void startAudio(View view) {
        Log.i(TAG, "startAudio() called =======================================");
        startAudio();
    }

    public void stopAudio(View view) {
        stopAudio();
    }

    public void pauseAudio(View view) {
        pauseAudio();
    }

    public void closeAudio(View view) {
        closeAudio();
    }

    public void openAudio() {
        try {
            StreamConfiguration requestedConfig = mStreamConfigurationView.getRequestedConfiguration();
            requestedConfig.setFramesPerBurst(audioManagerFramesPerBurst);
            mAudioStreamTester.open(requestedConfig,
                    mStreamConfigurationView.getActualConfiguration());
            mState = STATE_OPEN;
            int sessionId = mStreamConfigurationView.getActualConfiguration().getSessionId();
            if (sessionId > 0) {
                setupEffects(sessionId);
            }
            mStreamConfigurationView.updateDisplay();
            updateEnabledWidgets();
            mStreamSniffer.startStreamSniffer();
        } catch (Exception e) {
            e.printStackTrace();
            mStatusView.setText(e.getMessage());
            showToast(e.getMessage());
        }
    }

    public void startAudio() {
        try {
            mAudioStreamTester.start();
            mState = STATE_STARTED;
            mStreamConfigurationView.updateDisplay();
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            mStatusView.setText(e.getMessage());
            showToast(e.getMessage());
        }
    }

    public void pauseAudio() {
        try {
            mAudioStreamTester.pause();
            mState = STATE_PAUSED;
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            mStatusView.setText(e.getMessage());
            showToast(e.getMessage());
        }
    }

    public void stopAudio() {
        try {
            mAudioStreamTester.stop();
            mState = STATE_STOPPED;
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            mStatusView.setText(e.getMessage());
            showToast(e.getMessage());
        }
    }

    public void closeAudio() {
        mStreamSniffer.stopStreamSniffer();
        mAudioStreamTester.close();
        mState = STATE_CLOSED;
        updateEnabledWidgets();
    }
}
