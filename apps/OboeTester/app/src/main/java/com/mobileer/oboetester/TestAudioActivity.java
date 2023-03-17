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

package com.mobileer.oboetester;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Locale;

/**
 * Base class for other Activities.
 */
abstract class TestAudioActivity extends Activity {
    public static final String TAG = "OboeTester";

    protected static final int FADER_PROGRESS_MAX = 1000;
    private static final int INTENT_TEST_DELAY_MILLIS = 1100;

    public static final int AUDIO_STATE_OPEN = 0;
    public static final int AUDIO_STATE_STARTED = 1;
    public static final int AUDIO_STATE_PAUSED = 2;
    public static final int AUDIO_STATE_STOPPED = 3;
    public static final int AUDIO_STATE_RELEASED = 4;
    public static final int AUDIO_STATE_CLOSING = 5;
    public static final int AUDIO_STATE_CLOSED = 6;

    public static final int COLOR_ACTIVE = 0xFFD0D0A0;
    public static final int COLOR_IDLE = 0xFFD0D0D0;

    // Pass the activity index to native so it can know how to respond to the start and stop calls.
    // WARNING - must match definitions in NativeAudioContext.h ActivityType
    public static final int ACTIVITY_TEST_OUTPUT = 0;
    public static final int ACTIVITY_TEST_INPUT = 1;
    public static final int ACTIVITY_TAP_TO_TONE = 2;
    public static final int ACTIVITY_RECORD_PLAY = 3;
    public static final int ACTIVITY_ECHO = 4;
    public static final int ACTIVITY_RT_LATENCY = 5;
    public static final int ACTIVITY_GLITCHES = 6;
    public static final int ACTIVITY_TEST_DISCONNECT = 7;
    public static final int ACTIVITY_DATA_PATHS = 8;

    private static final int MY_PERMISSIONS_REQUEST_EXTERNAL_STORAGE = 1001;

    private int mAudioState = AUDIO_STATE_CLOSED;

    protected ArrayList<StreamContext> mStreamContexts;
    private Button mOpenButton;
    private Button mStartButton;
    private Button mPauseButton;
    private Button mStopButton;
    private Button mReleaseButton;
    private Button mCloseButton;
    private MyStreamSniffer mStreamSniffer;
    private CheckBox mCallbackReturnStopBox;
    private int mSampleRate;
    private int mSingleTestIndex = -1;
    private static boolean mBackgroundEnabled;

    protected Bundle mBundleFromIntent;
    protected boolean mTestRunningByIntent;
    protected String mResultFileName;
    private String mTestResults;

    public String getTestName() {
        return "TestAudio";
    }

    public static class StreamContext {
        StreamConfigurationView configurationView;
        AudioStreamTester tester;

        boolean isInput() {
            return tester.getCurrentAudioStream().isInput();
        }
    }

    // Periodically query the status of the streams.
    protected class MyStreamSniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private Handler mHandler;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                boolean streamClosed = false;
                boolean gotViews = false;
                for (StreamContext streamContext : mStreamContexts) {
                    AudioStreamBase.StreamStatus status = streamContext.tester.getCurrentAudioStream().getStreamStatus();
                    AudioStreamBase.DoubleStatistics latencyStatistics =
                            streamContext.tester.getCurrentAudioStream().getLatencyStatistics();
                    if (streamContext.configurationView != null) {
                        // Handler runs this on the main UI thread.
                        int framesPerBurst = streamContext.tester.getCurrentAudioStream().getFramesPerBurst();
                        status.framesPerCallback = getFramesPerCallback();
                        String msg = "";
                        msg += "timestamp.latency = " + latencyStatistics.dump() + "\n";
                        msg += status.dump(framesPerBurst);
                        streamContext.configurationView.setStatusText(msg);
                        updateStreamDisplay();
                        gotViews = true;
                    }

                    streamClosed = streamClosed || (status.state >= 12);
                }

                if (streamClosed) {
                    onStreamClosed();
                } else {
                    // Repeat this runnable code block again.
                    if (gotViews) {
                        mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
                    }
                }
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

    public static void setBackgroundEnabled(boolean enabled) {
        mBackgroundEnabled = enabled;
    }

    public static boolean isBackgroundEnabled() {
        return mBackgroundEnabled;
    }

    public void onStreamClosed() {
    }

    protected abstract void inflateActivity();

    void updateStreamDisplay() {
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        inflateActivity();
        findAudioCommon();

        mBundleFromIntent = getIntent().getExtras();
    }

    @Override
    public void onNewIntent(Intent intent) {
        mBundleFromIntent = intent.getExtras();
    }

    public boolean isTestConfiguredUsingBundle() {
        return mBundleFromIntent != null;
    }

    public void hideSettingsViews() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.hideSettingsView();
            }
        }
    }

    abstract int getActivityType();

    public void setSingleTestIndex(int testIndex) {
        mSingleTestIndex = testIndex;
    }

    public int getSingleTestIndex() {
        return mSingleTestIndex;
    }

    @Override
    protected void onStart() {
        super.onStart();
        resetConfiguration();
        setActivityType(getActivityType());
    }

    protected void resetConfiguration() {
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mBundleFromIntent != null) {
            processBundleFromIntent();
        }
    }

    private void setVolumeFromIntent() {
        float normalizedVolume = IntentBasedTestSupport.getNormalizedVolumeFromBundle(mBundleFromIntent);
        if (normalizedVolume >= 0.0) {
            int streamType = IntentBasedTestSupport.getVolumeStreamTypeFromBundle(mBundleFromIntent);
            AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            int maxVolume = audioManager.getStreamMaxVolume(streamType);
            int requestedVolume = (int) (maxVolume * normalizedVolume);
            audioManager.setStreamVolume(streamType, requestedVolume, 0);
        }
    }

    private void processBundleFromIntent() {
        if (mTestRunningByIntent) {
            return;
        }

        // Delay the test start to avoid race conditions. See Oboe Issue #1533
        mTestRunningByIntent = true;
        Handler handler = new Handler(Looper.getMainLooper()); // UI thread
        handler.postDelayed(new DelayedTestByIntentRunnable(),
                INTENT_TEST_DELAY_MILLIS); // Delay long enough to get past the onStop() call!

    }

    private class DelayedTestByIntentRunnable implements Runnable {
        @Override
        public void run() {
            try {
                mResultFileName = mBundleFromIntent.getString(IntentBasedTestSupport.KEY_FILE_NAME);
                setVolumeFromIntent();
                startTestUsingBundle();
            } catch( Exception e) {
                showErrorToast(e.getMessage());
            }
        }
    }

    public void startTestUsingBundle() {
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onStop() {
        if (!isBackgroundEnabled()) {
            Log.i(TAG, "onStop() called so stop the test =========================");
            onStopTest();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (isBackgroundEnabled()) {
            Log.i(TAG, "onDestroy() called so stop the test =========================");
            onStopTest();
        }
        mAudioState = AUDIO_STATE_CLOSED;
        super.onDestroy();
    }

    protected void updateEnabledWidgets() {
        if (mOpenButton != null) {
            mOpenButton.setBackgroundColor(mAudioState == AUDIO_STATE_OPEN ? COLOR_ACTIVE : COLOR_IDLE);
            mStartButton.setBackgroundColor(mAudioState == AUDIO_STATE_STARTED ? COLOR_ACTIVE : COLOR_IDLE);
            mPauseButton.setBackgroundColor(mAudioState == AUDIO_STATE_PAUSED ? COLOR_ACTIVE : COLOR_IDLE);
            mStopButton.setBackgroundColor(mAudioState == AUDIO_STATE_STOPPED ? COLOR_ACTIVE : COLOR_IDLE);
            mReleaseButton.setBackgroundColor(mAudioState == AUDIO_STATE_RELEASED ? COLOR_ACTIVE : COLOR_IDLE);
            mCloseButton.setBackgroundColor(mAudioState == AUDIO_STATE_CLOSED ? COLOR_ACTIVE : COLOR_IDLE);
        }
        setConfigViewsEnabled(mAudioState == AUDIO_STATE_CLOSED);
    }

    private void setConfigViewsEnabled(boolean b) {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.setChildrenEnabled(b);
            }
        }
    }

    private void applyConfigurationViewsToModels() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.applyToModel(streamContext.tester.requestedConfiguration);
            }
        }
    }

    abstract boolean isOutput();

    public void clearStreamContexts() {
        mStreamContexts.clear();
    }

    public StreamContext addOutputStreamContext() {
        StreamContext streamContext = new StreamContext();
        streamContext.tester = AudioOutputTester.getInstance();
        streamContext.configurationView = (StreamConfigurationView)
                findViewById(R.id.outputStreamConfiguration);
        if (streamContext.configurationView == null) {
            streamContext.configurationView = (StreamConfigurationView)
                    findViewById(R.id.streamConfiguration);
        }
        if (streamContext.configurationView != null) {
            streamContext.configurationView.setOutput(true);
        }
        mStreamContexts.add(streamContext);
        return streamContext;
    }

    public AudioOutputTester addAudioOutputTester() {
        StreamContext streamContext = addOutputStreamContext();
        return (AudioOutputTester) streamContext.tester;
    }

    public StreamContext addInputStreamContext() {
        StreamContext streamContext = new StreamContext();
        streamContext.tester = AudioInputTester.getInstance();
        streamContext.configurationView = (StreamConfigurationView)
                findViewById(R.id.inputStreamConfiguration);
        if (streamContext.configurationView == null) {
            streamContext.configurationView = (StreamConfigurationView)
                    findViewById(R.id.streamConfiguration);
        }
        if (streamContext.configurationView != null) {
            streamContext.configurationView.setOutput(false);
        }
        streamContext.tester = AudioInputTester.getInstance();
        mStreamContexts.add(streamContext);
        return streamContext;
    }

    public AudioInputTester addAudioInputTester() {
        StreamContext streamContext = addInputStreamContext();
        return (AudioInputTester) streamContext.tester;
    }

    void updateStreamConfigurationViews() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.updateDisplay(streamContext.tester.actualConfiguration);
            }
        }
    }

    StreamContext getFirstInputStreamContext() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.isInput())
                return streamContext;
        }
        return null;
    }

    StreamContext getFirstOutputStreamContext() {
        for (StreamContext streamContext : mStreamContexts) {
            if (!streamContext.isInput())
                return streamContext;
        }
        return null;
    }

    protected void findAudioCommon() {
        mOpenButton = (Button) findViewById(R.id.button_open);
        if (mOpenButton != null) {
            mStartButton = (Button) findViewById(R.id.button_start);
            mPauseButton = (Button) findViewById(R.id.button_pause);
            mStopButton = (Button) findViewById(R.id.button_stop);
            mReleaseButton = (Button) findViewById(R.id.button_release);
            mCloseButton = (Button) findViewById(R.id.button_close);
        }
        mStreamContexts = new ArrayList<StreamContext>();

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

    private void updateNativeAudioParameters() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            String text = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            int audioManagerSampleRate = Integer.parseInt(text);
            text = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            int audioManagerFramesPerBurst = Integer.parseInt(text);
            setDefaultAudioValues(audioManagerSampleRate, audioManagerFramesPerBurst);
        }
    }

    protected void showErrorToast(String message) {
        String text = "Error: " + message;
        Log.e(TAG, text);
        showToast(text);
    }

    protected void showToast(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(TestAudioActivity.this,
                        message,
                        Toast.LENGTH_SHORT).show();
            }
        });
    }

    public void openAudio(View view) {
        try {
            openAudio();
        } catch (Exception e) {
            showErrorToast(e.getMessage());
        }
    }

    public void startAudio(View view) {
        Log.i(TAG, "startAudio() called =======================================");
        try {
            startAudio();
        } catch (Exception e) {
            showErrorToast(e.getMessage());
        }
        keepScreenOn(true);
    }

    protected void keepScreenOn(boolean on) {
        if (on) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }

    public void stopAudio(View view) {
        stopAudio();
        keepScreenOn(false);
    }

    public void pauseAudio(View view) {
        pauseAudio();
        keepScreenOn(false);
    }

    public void closeAudio(View view) {
        closeAudio();
    }

    public void releaseAudio(View view) {
        releaseAudio();
    }

    public int getSampleRate() {
        return mSampleRate;
    }

    public void openAudio() throws IOException {
        closeAudio();

        updateNativeAudioParameters();

        if (!isTestConfiguredUsingBundle()) {
            applyConfigurationViewsToModels();
        }

        int sampleRate = 0;

        // Open output streams then open input streams.
        // This is so that the capacity of input stream can be expanded to
        // match the burst size of the output for full duplex.
        for (StreamContext streamContext : mStreamContexts) {
            if (!streamContext.isInput()) {
                openStreamContext(streamContext);
                int streamSampleRate = streamContext.tester.actualConfiguration.getSampleRate();
                if (sampleRate == 0) {
                    sampleRate = streamSampleRate;
                }
            }
        }
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.isInput()) {
                if (sampleRate != 0) {
                    streamContext.tester.requestedConfiguration.setSampleRate(sampleRate);
                }
                openStreamContext(streamContext);
            }
        }
        updateEnabledWidgets();
        mStreamSniffer.startStreamSniffer();
    }

    /**
     * @param deviceId
     * @return true if the device is TYPE_BLUETOOTH_SCO
     */
    boolean isScoDevice(int deviceId) {
        if (deviceId == 0) return false; // Unspecified
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        final AudioDeviceInfo[] devices = audioManager.getDevices(
                AudioManager.GET_DEVICES_INPUTS | AudioManager.GET_DEVICES_OUTPUTS);
        for (AudioDeviceInfo device : devices) {
            if (device.getId() == deviceId) {
                return device.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
            }
        }
        return false;
    }

    private void openStreamContext(StreamContext streamContext) throws IOException {
        StreamConfiguration requestedConfig = streamContext.tester.requestedConfiguration;
        StreamConfiguration actualConfig = streamContext.tester.actualConfiguration;

        streamContext.tester.open(); // OPEN the stream

        mSampleRate = actualConfig.getSampleRate();
        mAudioState = AUDIO_STATE_OPEN;
        int sessionId = actualConfig.getSessionId();
        if (streamContext.configurationView != null) {
            if (sessionId > 0) {
                try {
                    streamContext.configurationView.setupEffects(sessionId);
                } catch (Exception e) {
                    showErrorToast(e.getMessage());
                }
            }
            streamContext.configurationView.updateDisplay(streamContext.tester.actualConfiguration);
        }
    }

    // Native methods
    private native int startNative();

    private native int pauseNative();

    private native int stopNative();

    private native int releaseNative();

    protected native void setActivityType(int activityType);

    private native int getFramesPerCallback();

    private static native void setDefaultAudioValues(int audioManagerSampleRate, int audioManagerFramesPerBurst);

    public void startAudio() throws IOException {
        Log.i(TAG, "startAudio() called =========================");
        int result = startNative();
        if (result != 0) {
            showErrorToast("Start failed with " + result);
            throw new IOException("startNative returned " + result);
        } else {
            for (StreamContext streamContext : mStreamContexts) {
                StreamConfigurationView configView = streamContext.configurationView;
                if (configView != null) {
                    configView.updateDisplay(streamContext.tester.actualConfiguration);
                }
            }
            mAudioState = AUDIO_STATE_STARTED;
            updateEnabledWidgets();
        }
    }

    protected void toastPauseError(int result) {
        showErrorToast("Pause failed with " + result);
    }

    public void pauseAudio() {
        int result = pauseNative();
        if (result != 0) {
            toastPauseError(result);
        } else {
            mAudioState = AUDIO_STATE_PAUSED;
            updateEnabledWidgets();
        }
    }

    public void stopAudio() {
        int result = stopNative();
        if (result != 0) {
            showErrorToast("Stop failed with " + result);
        } else {
            mAudioState = AUDIO_STATE_STOPPED;
            updateEnabledWidgets();
        }
    }

    public void releaseAudio() {
        int result = releaseNative();
        if (result != 0) {
            showErrorToast("release failed with " + result);
        } else {
            mAudioState = AUDIO_STATE_RELEASED;
            updateEnabledWidgets();
        }
    }

    public void runTest() {
    }

    public void saveIntentLog() {
    }

    // This should only be called from UI events such as onStop or a button press.
    public void onStopTest() {
        stopTest();
    }

    public void stopTest() {
        stopAudio();
        closeAudio();
    }

    public void stopAudioQuiet() {
        stopNative();
        mAudioState = AUDIO_STATE_STOPPED;
        updateEnabledWidgets();
    }

    // Make synchronized so we don't close from two streams at the same time.
    public synchronized void closeAudio() {
        if (mAudioState >= AUDIO_STATE_CLOSING) {
            Log.d(TAG, "closeAudio() already closing");
            return;
        }
        mAudioState = AUDIO_STATE_CLOSING;

        mStreamSniffer.stopStreamSniffer();
        // Close output streams first because legacy callbacks may still be active
        // and an output stream may be calling the input stream.
        for (StreamContext streamContext : mStreamContexts) {
            if (!streamContext.isInput()) {
                streamContext.tester.close();
            }
        }
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.isInput()) {
                streamContext.tester.close();
            }
        }

        mAudioState = AUDIO_STATE_CLOSED;
        updateEnabledWidgets();
    }

    void startBluetoothSco() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        myAudioMgr.startBluetoothSco();
    }

    void stopBluetoothSco() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        myAudioMgr.stopBluetoothSco();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {

        if (MY_PERMISSIONS_REQUEST_EXTERNAL_STORAGE != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }
        // If request is cancelled, the result arrays are empty.
        if (grantResults.length > 0
                && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            writeTestResult(mTestResults);
        } else {
            showToast("Writing external storage needed for test results.");
        }
    }

    @NonNull
    protected String getCommonTestReport() {
        StringBuffer report = new StringBuffer();
        // Add some extra information for the remote tester.
        report.append("build.fingerprint = " + Build.FINGERPRINT + "\n");
        try {
            PackageInfo pinfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            report.append(String.format(Locale.getDefault(), "test.version = %s\n", pinfo.versionName));
            report.append(String.format(Locale.getDefault(), "test.version.code = %d\n", pinfo.versionCode));
        } catch (PackageManager.NameNotFoundException e) {
        }
        report.append("time.millis = " + System.currentTimeMillis() + "\n");

        if (mStreamContexts.size() == 0) {
            report.append("ERROR: no active streams" + "\n");
        } else {
            StreamContext streamContext = mStreamContexts.get(0);
            AudioStreamTester streamTester = streamContext.tester;
            report.append(streamTester.actualConfiguration.dump());
            AudioStreamBase.StreamStatus status = streamTester.getCurrentAudioStream().getStreamStatus();
            AudioStreamBase.DoubleStatistics latencyStatistics =
                    streamTester.getCurrentAudioStream().getLatencyStatistics();
            int framesPerBurst = streamTester.getCurrentAudioStream().getFramesPerBurst();
            status.framesPerCallback = getFramesPerCallback();
            report.append("timestamp.latency = " + latencyStatistics.dump() + "\n");
            report.append(status.dump(framesPerBurst));
        }

        return report.toString();
    }

    void writeTestResultIfPermitted(String resultString) {
        // Here, thisActivity is the current activity
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            mTestResults = resultString;
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    MY_PERMISSIONS_REQUEST_EXTERNAL_STORAGE);
        } else {
            // Permission has already been granted
            writeTestResult(resultString);
        }
    }

    void maybeWriteTestResult(String resultString) {
        if (mResultFileName != null) {
            writeTestResultIfPermitted(resultString);
        };
    }

    // Run this in a background thread.
    void writeTestResult(String resultString) {
        File resultFile = new File(mResultFileName);
        Writer writer = null;
        try {
            writer = new OutputStreamWriter(new FileOutputStream(resultFile));
            writer.write(resultString);
        } catch (
                IOException e) {
            e.printStackTrace();
            showErrorToast(" writing result file. " + e.getMessage());
        } finally {
            if (writer != null) {
                try {
                    writer.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        mResultFileName = null;
    }
}
