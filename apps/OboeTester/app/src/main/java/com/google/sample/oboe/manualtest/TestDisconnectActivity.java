/*
 * Copyright 2019 The Android Open Source Project
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.IOException;
import java.util.Date;

/**
 * Guide the user through a series of tests plugging in and unplugging a headset.
 * Print a summary at the end of any failures.
 *
 * TODO Test Input
 */
public class TestDisconnectActivity extends TestAudioActivity implements Runnable {

    private static final String TEXT_SKIP = "SKIP";
    private static final String TEXT_PASS = "PASS";
    private static final String TEXT_FAIL = "FAIL !!!!";
    public static final int POLL_DURATION_MILLIS = 50;
    public static final int SETTLING_TIME_MILLIS = 600;
    public static final int TIME_TO_FAILURE_MILLIS = 3000;

    private TextView     mInstructionsTextView;
    private TextView     mAutoTextView;
    private TextView     mStatusTextView;
    private TextView     mPlugTextView;

    private Thread       mAutoThread;
    private volatile boolean mThreadEnabled;
    private volatile boolean mTestFailed;
    private volatile boolean mSkipTest;
    private volatile int mPlugCount;
    private int          mTestCount;
    private StringBuffer mFailedSummary;
    private int          mPassCount;
    private int          mFailCount;
    private BroadcastReceiver mPluginReceiver = new PluginBroadcastReceiver();
    private Button       mStartButton;
    private Button       mStopButton;
    private Button       mShareButton;
    private Button       mFailButton;
    private Button       mSkipButton;

    // Receive a broadcast Intent when a headset is plugged in or unplugged.
    // Display a count on screen.
    public class PluginBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            mPlugCount++;
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String message = "Intent.HEADSET_PLUG #" + mPlugCount;
                    mPlugTextView.setText(message);
                }
            });
        }
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_disconnect);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mInstructionsTextView = (TextView) findViewById(R.id.text_instructions);
        mStatusTextView = (TextView) findViewById(R.id.text_status);
        mPlugTextView = (TextView) findViewById(R.id.text_plug_events);
        mAutoTextView = (TextView) findViewById(R.id.text_log);
        mAutoTextView.setMovementMethod(new ScrollingMovementMethod());

        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setEnabled(false);
        mFailButton = (Button) findViewById(R.id.button_fail);
        mSkipButton = (Button) findViewById(R.id.button_skip);
        updateStartStopButtons(false);
        updateFailSkipButton(false);
    }

    private void updateStartStopButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
    }

    @Override
    protected void onStart() {
        super.onStart();
        setActivityType(ACTIVITY_TEST_DISCONNECT);
    }

    @Override
    boolean isOutput() {
        return true;
    }

    @Override
    public void setupEffects(int sessionId) {
    }

    private void updateFailSkipButton(final boolean running) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mFailButton.setEnabled(running);
                mSkipButton.setEnabled(running);
            }
        });
    }

    // Write to scrollable TextView
    private void log(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAutoTextView.append(text);
                mAutoTextView.append("\n");
            }
        });
    }

    // Write to status and command view
    private void setInstructionsText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mInstructionsTextView.setText(text);
            }
        });
    }

    // Write to status and command view
    private void setStatusText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStatusTextView.setText(text);
            }
        });
    }

    private void logClear() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAutoTextView.setText("");
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
        this.registerReceiver(mPluginReceiver, filter);
    }

    @Override
    public void onPause() {
        this.unregisterReceiver(mPluginReceiver);
        super.onPause();
    }

    // Only call from UI thread.
    public void onTestFinished() {
        updateStartStopButtons(false);
        mShareButton.setEnabled(true);
    }

    public void startAudioTest() throws IOException {
        openAudio();
        startAudio();
    }

    public void stopAudioTest() {
        stopAudioQuiet();
        closeAudio();
    }

    public void onCancel(View view) {
        stopAudioTest();
        onTestFinished();
    }

    // Called on UI thread
    public void onStopAudioTest(View view) {
        stopAudioTest();
        onTestFinished();
        keepScreenOn(false);
    }

    public void onStartDisconnectTest(View view) {
        updateStartStopButtons(true);
        mThreadEnabled = true;
        mAutoThread = new Thread(this);
        mAutoThread.start();
    }

    public void onStopDisconnectTest(View view) {
        try {
            if (mAutoThread != null) {
                mThreadEnabled = false;
                mAutoThread.interrupt();
                mAutoThread.join(100);
                mAutoThread = null;
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void onFailTest(View view) {
        mTestFailed = true;
    }

    public void onSkipTest(View view) {
        mSkipTest = true;
    }

    // Share text from log via GMail, Drive or other method.
    public void onShareResult(View view) {
        Intent sharingIntent = new Intent(Intent.ACTION_SEND);
        sharingIntent.setType("text/plain");

        String subjectText = "OboeTester Test Disconnect result " + getTimestampString();
        sharingIntent.putExtra(Intent.EXTRA_SUBJECT, subjectText);

        String shareBody = mAutoTextView.getText().toString();
        sharingIntent.putExtra(Intent.EXTRA_TEXT, shareBody);

        startActivity(Intent.createChooser(sharingIntent, "Share using:"));
    }

    private String getConfigText(StreamConfiguration config) {
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "IN")
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode());
    }

    private void testConfiguration(boolean isInput,
                                   int perfMode,
                                   int sharingMode,
                                   int channelCount,
                                   boolean requestPlugin) throws InterruptedException {
        String actualConfigText = "none";
        mSkipTest = false;

        AudioInputTester    mAudioInTester = null;
        AudioOutputTester   mAudioOutTester = null;

        clearStreamContexts();

        if (isInput) {
            mAudioInTester = addAudioInputTester();
        } else {
            mAudioOutTester = addAudioOutputTester();
        }

        // Configure settings
        StreamConfiguration requestedConfig = (isInput)
                ? mAudioInTester.requestedConfiguration
                : mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualConfig = (isInput)
                ? mAudioInTester.actualConfiguration
                : mAudioOutTester.actualConfiguration;

        requestedConfig.reset();
        requestedConfig.setPerformanceMode(perfMode);
        requestedConfig.setSharingMode(sharingMode);
        requestedConfig.setChannelCount(channelCount);

        log("========================== #" + mTestCount);
        log("Requested:");
        log(getConfigText(requestedConfig));

        // Give previous stream time to close and release resources. Avoid race conditions.
        Thread.sleep(SETTLING_TIME_MILLIS);
        if (!mThreadEnabled) return;
        boolean openFailed = false;
        AudioStreamBase stream = null;
        try {
            startAudioTest(); // this will fill in actualConfig
            log("Actual:");
            actualConfigText = getConfigText(actualConfig)
                    + ", " + (actualConfig.isMMap() ? "MMAP" : "Legacy");
            log(actualConfigText);

            stream = (isInput)
                    ? mAudioInTester.getCurrentAudioStream()
                    : mAudioOutTester.getCurrentAudioStream();
        } catch (IOException e) {
            openFailed = true;
            log(e.getMessage());
        }

        // The test is only worth running if we got the configuration we requested.
        boolean valid = true;
        if (!openFailed) {
            if(actualConfig.getSharingMode() != sharingMode) {
                log("did not get requested sharing mode");
                valid = false;
            }
            if (actualConfig.getPerformanceMode() != perfMode) {
                log("did not get requested performance mode");
                valid = false;
            }
            if (actualConfig.getNativeApi() == StreamConfiguration.NATIVE_API_OPENSLES) {
                log("OpenSL ES does not support automatic disconnect");
                valid = false;
            }
        }

        int oldPlugCount = mPlugCount;
        if (!openFailed && valid) {
            mTestFailed = false;
            updateFailSkipButton(true);
            // poll for stream disconnected
            while (!mTestFailed && mThreadEnabled && !mSkipTest &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTING) {
                Thread.sleep(POLL_DURATION_MILLIS);
            }
            String message = (requestPlugin ? "Plug IN" : "UNplug") + " headset now!";
            setStatusText("Testing:\n" + actualConfigText);
            setInstructionsText(message);
            int timeoutCount = 0;
            // Wait for Java plug count to change or stream to disconnect.
            while (!mTestFailed && mThreadEnabled && !mSkipTest &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTED) {
                Thread.sleep(POLL_DURATION_MILLIS);
                if (mPlugCount > oldPlugCount) {
                    timeoutCount = TIME_TO_FAILURE_MILLIS / POLL_DURATION_MILLIS;
                    break;
                }
            }
            // Wait for timeout or stream to disconnect.
            while (!mTestFailed && mThreadEnabled && !mSkipTest && (timeoutCount > 0) &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTED) {
                Thread.sleep(POLL_DURATION_MILLIS);
                timeoutCount--;
                if (timeoutCount == 0) {
                    mTestFailed = true;
                } else {
                    setStatusText("Plug detected by Java.\nCounting down to Oboe failure: " + timeoutCount);
                }
            }
            setStatusText(mTestFailed ? "Failed" : "Passed - detected");
        }
        updateFailSkipButton(false);
        setInstructionsText("Wait...");

        if (!openFailed) {
            stopAudioTest();
        }

        if (mSkipTest) valid = false;

        if (valid) {
            if (openFailed) {
                mFailedSummary.append("------ #" + mTestCount);
                mFailedSummary.append("\n");
                mFailedSummary.append(getConfigText(requestedConfig));
                mFailedSummary.append("\n");
                mFailedSummary.append("Open failed!\n");
                mFailCount++;
            } else {
                log("Result:");
                boolean passed = !mTestFailed;
                String resultText = requestPlugin ? "plugIN" : "UNplug";
                resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
                log(resultText);
                if (!passed) {
                    mFailedSummary.append("------ #" + mTestCount);
                    mFailedSummary.append("\n");
                    mFailedSummary.append("  ");
                    mFailedSummary.append(actualConfigText);
                    mFailedSummary.append("\n");
                    mFailedSummary.append("    ");
                    mFailedSummary.append(resultText);
                    mFailedSummary.append("\n");
                    mFailCount++;
                } else {
                    mPassCount++;
                }
            }
        } else {
            log(TEXT_SKIP);
        }
        // Give hardware time to settle between tests.
        Thread.sleep(1000);
        mTestCount++;
    }

    private void testConfiguration(boolean isInput, int performanceMode,
                                   int sharingMode) throws InterruptedException {
        int channelCount = 2;
        boolean requestPlugin = true; // plug IN
        testConfiguration(isInput, performanceMode, sharingMode, channelCount, requestPlugin);
        requestPlugin = false; // UNplug
        testConfiguration(isInput, performanceMode, sharingMode, channelCount, requestPlugin);
    }

    private void testConfiguration(int performanceMode,
                                   int sharingMode) throws InterruptedException {
        testConfiguration(false, performanceMode, sharingMode);
        testConfiguration(true, performanceMode, sharingMode);
    }

    @Override
    public void run() {
        mPlugCount = 0;
        logClear();
        log("=== STARTED at " + new Date());
        log(Build.MANUFACTURER + " " + Build.PRODUCT);
        log(Build.DISPLAY);
        mFailedSummary = new StringBuffer();
        mTestCount = 0;
        mPassCount = 0;
        mFailCount = 0;
        // Try several different configurations.
        try {
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                        StreamConfiguration.SHARING_MODE_EXCLUSIVE);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                        StreamConfiguration.SHARING_MODE_SHARED);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_NONE,
                        StreamConfiguration.SHARING_MODE_SHARED);
        } catch (InterruptedException e) {
            e.printStackTrace();
        } finally {
            stopAudioTest();
            setInstructionsText("See summary below.");
            setStatusText("Finished.");
            log("\n==== SUMMARY ========");
            if (mFailCount > 0) {
                log(mPassCount + " passed. " + mFailCount + " failed.");
                log("These tests FAILED:");
                log(mFailedSummary.toString());
            } else {
                log("All tests PASSED.");
            }
            log("== FINISHED at " + new Date());
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    onTestFinished();
                }
            });
            updateFailSkipButton(false);
        }
    }

}
