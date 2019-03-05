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
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Toast;

import java.io.IOException;
import java.util.ArrayList;

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
    protected String audioManagerSampleRate;
    protected int audioManagerFramesPerBurst;
    protected ArrayList<StreamContext> mStreamContexts;
    private Button mOpenButton;
    private Button mStartButton;
    private Button mPauseButton;
    private Button mStopButton;
    private Button mCloseButton;
    private MyStreamSniffer mStreamSniffer;
    private CheckBox mCallbackReturnStopBox;

    public static class StreamContext {
        StreamConfigurationView configurationView;
        AudioStreamTester tester;
    }

    // Periodically query the status of the stream.
    protected class MyStreamSniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private Handler mHandler;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {

                for (StreamContext streamContext : mStreamContexts) {
                    // Handler runs this on the main UI thread.
                    AudioStreamBase.StreamStatus status = streamContext.tester.getCurrentAudioStream().getStreamStatus();
                    int framesPerBurst = streamContext.tester.getCurrentAudioStream().getFramesPerBurst();
                    final String msg = status.dump(framesPerBurst);
                    mStreamContexts.get(0).configurationView.setStatusText(msg);
                    updateStreamDisplay();
                }

                // Repeat this runnable code block again.
                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void startStreamSniffer() {
            stopStreamSniffer();
            mHandler = new Handler(Looper.getMainLooper());
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopStreamSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }

    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        inflateActivity();
        findAudioCommon();
    }

    protected abstract void inflateActivity();

    void updateStreamDisplay() {
    }

    @Override
    protected void onDestroy() {
        try {
            for (StreamContext streamContext : mStreamContexts) {
                streamContext.tester.stop();
            }
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
        setConfigViewsEnabled(mState == STATE_CLOSED);
    }

    private void setConfigViewsEnabled(boolean b) {
        for (StreamContext streamContext : mStreamContexts) {
            streamContext.configurationView.setChildrenEnabled(b);
        }
    }

    abstract boolean isOutput();

    public AudioOutputTester addAudioOutputTester() {
        StreamContext streamContext = new StreamContext();
        streamContext.configurationView =(StreamConfigurationView)
                findViewById(R.id.outputStreamConfiguration);
        if (streamContext.configurationView == null) {
            streamContext.configurationView =(StreamConfigurationView)
                    findViewById(R.id.streamConfiguration);
        }
        streamContext.configurationView.setOutput(true);
        streamContext.tester = AudioOutputTester.getInstance();
        mStreamContexts.add(streamContext);
        return (AudioOutputTester) streamContext.tester;
    }

    public AudioInputTester addAudioInputTester() {
        StreamContext streamContext = new StreamContext();
        streamContext.configurationView =(StreamConfigurationView)
                findViewById(R.id.inputStreamConfiguration);
        if (streamContext.configurationView == null) {
            streamContext.configurationView =(StreamConfigurationView)
                    findViewById(R.id.streamConfiguration);
        }
        streamContext.configurationView.setOutput(false);
        streamContext.tester = AudioInputTester.getInstance();
        mStreamContexts.add(streamContext);
        return (AudioInputTester) streamContext.tester;
    }

    void updateStreamConfigurationViews() {
        for (StreamContext streamContext : mStreamContexts) {
            streamContext.configurationView.updateDisplay();
        }
    }

    protected void findAudioCommon() {
        mOpenButton = (Button) findViewById(R.id.button_open);
        if (mOpenButton != null) {
            mStartButton = (Button) findViewById(R.id.button_start);
            mPauseButton = (Button) findViewById(R.id.button_pause);
            mStopButton = (Button) findViewById(R.id.button_stop);
            mCloseButton = (Button) findViewById(R.id.button_close);
        }
        mStreamContexts = new ArrayList<StreamContext>();

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
            for (StreamContext streamContext : mStreamContexts) {
                StreamConfigurationView configView = streamContext.configurationView;
                StreamConfiguration requestedConfig = configView.getRequestedConfiguration();
                requestedConfig.setFramesPerBurst(audioManagerFramesPerBurst);
                streamContext.tester.open(requestedConfig, configView.getActualConfiguration());
                mState = STATE_OPEN;
                int sessionId = configView.getActualConfiguration().getSessionId();
                if (sessionId > 0) {
                    setupEffects(sessionId);
                }
                configView.updateDisplay();
            }
            updateEnabledWidgets();
            mStreamSniffer.startStreamSniffer();
        } catch (Exception e) {
            e.printStackTrace();
            showToast(e.getMessage());
        }
    }

    public void startAudio() {
        try {
            mState = STATE_STARTED;
            for (StreamContext streamContext : mStreamContexts) {
                StreamConfigurationView configView = streamContext.configurationView;
                streamContext.tester.start();
                configView.updateDisplay();
            }
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            showToast(e.getMessage());
        }
    }

    public void pauseAudio() {
        try {
            for (StreamContext streamContext : mStreamContexts) {
                streamContext.tester.pause();
            }
            mState = STATE_PAUSED;
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            showToast(e.getMessage());
        }
    }

    public void stopAudio() {
        try {
            for (StreamContext streamContext : mStreamContexts) {
                streamContext.tester.stop();
            }
            mState = STATE_STOPPED;
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            showToast(e.getMessage());
        }
    }

    public void closeAudio() {
        mStreamSniffer.stopStreamSniffer();
        for (StreamContext streamContext : mStreamContexts) {
            streamContext.tester.close();
        }
        mState = STATE_CLOSED;
        updateEnabledWidgets();
    }
}
