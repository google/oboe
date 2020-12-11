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
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.IOException;

/**
 * Guide the user through a series of tests plugging in and unplugging a headset.
 * Print a summary at the end of any failures.
 */
public class TestDisconnectActivity extends TestAudioActivity {

    private static final String TEXT_SKIP = "SKIP";
    private static final String TEXT_PASS = "PASS";
    private static final String TEXT_FAIL = "FAIL !!!!";
    public static final int POLL_DURATION_MILLIS = 50;
    public static final int SETTLING_TIME_MILLIS = 600;
    public static final int TIME_TO_FAILURE_MILLIS = 3000;

    private TextView     mInstructionsTextView;
    private TextView     mStatusTextView;
    private TextView     mPlugTextView;

    private volatile boolean mTestFailed;
    private volatile boolean mSkipTest;
    private volatile int mPlugCount;
    private BroadcastReceiver mPluginReceiver = new PluginBroadcastReceiver();
    private Button       mFailButton;
    private Button       mSkipButton;

    protected AutomatedTestRunner mAutomatedTestRunner;

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

        mAutomatedTestRunner = findViewById(R.id.auto_test_runner);
        mAutomatedTestRunner.setActivity(this);

        mInstructionsTextView = (TextView) findViewById(R.id.text_instructions);
        mStatusTextView = (TextView) findViewById(R.id.text_status);
        mPlugTextView = (TextView) findViewById(R.id.text_plug_events);

        mFailButton = (Button) findViewById(R.id.button_fail);
        mSkipButton = (Button) findViewById(R.id.button_skip);
        updateFailSkipButton(false);
    }

    @Override
    public String getTestName() {
        return "Disconnect";
    }

    int getActivityType() {
        return ACTIVITY_TEST_DISCONNECT;
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

    // This should only be called from UI events such as onStop or a button press.
    @Override
    public void onStopTest() {
        mAutomatedTestRunner.stopTest();
    }

    public void startAudioTest() throws IOException {
        startAudio();
    }

    public void stopAudioTest() {
        stopAudioQuiet();
        closeAudio();
    }

    public void onCancel(View view) {
        stopAudioTest();
        mAutomatedTestRunner.onTestFinished();
    }

    // Called on UI thread
    public void onStopAudioTest(View view) {
        stopAudioTest();
        mAutomatedTestRunner.onTestFinished();
        keepScreenOn(false);
    }

    public void onFailTest(View view) {
        mTestFailed = true;
    }

    public void onSkipTest(View view) {
        mSkipTest = true;
    }

    private String getConfigText(StreamConfiguration config) {
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "IN")
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", " + config.getSampleRate();
    }

    private void log(String text) {
        mAutomatedTestRunner.log(text);
    }

    private void appendFailedSummary(String text) {
        mAutomatedTestRunner.appendFailedSummary(text);
    }

    private void testConfiguration(boolean isInput,
                                   int perfMode,
                                   int sharingMode,
                                   int sampleRate,
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
        requestedConfig.setSampleRate(sampleRate);
        if (sampleRate != 0) {
            requestedConfig.setRateConversionQuality(StreamConfiguration.RATE_CONVERSION_QUALITY_MEDIUM);
        }

        log("========================== #" + mAutomatedTestRunner.getTestCount());
        log("Requested:");
        log(getConfigText(requestedConfig));

        // Give previous stream time to close and release resources. Avoid race conditions.
        Thread.sleep(SETTLING_TIME_MILLIS);
        if (!mAutomatedTestRunner.isThreadEnabled()) return;
        boolean openFailed = false;
        AudioStreamBase stream = null;
        try {
            openAudio();
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

        if (!openFailed && valid) {
            try {
                startAudioTest();
            } catch (IOException e) {
                e.printStackTrace();
                valid = false;
                log(e.getMessage());
            }
        }

        int oldPlugCount = mPlugCount;
        if (!openFailed && valid) {
            mTestFailed = false;
            updateFailSkipButton(true);
            // poll until stream started
            while (!mTestFailed && mAutomatedTestRunner.isThreadEnabled() && !mSkipTest &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTING) {
                Thread.sleep(POLL_DURATION_MILLIS);
            }
            String message = (requestPlugin ? "Plug IN" : "UNplug") + " headset now!";
            setStatusText("Testing:\n" + actualConfigText);
            setInstructionsText(message);
            int timeoutCount = 0;
            // Wait for Java plug count to change or stream to disconnect.
            while (!mTestFailed && mAutomatedTestRunner.isThreadEnabled() && !mSkipTest &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTED) {
                Thread.sleep(POLL_DURATION_MILLIS);
                if (mPlugCount > oldPlugCount) {
                    timeoutCount = TIME_TO_FAILURE_MILLIS / POLL_DURATION_MILLIS;
                    break;
                }
            }
            // Wait for timeout or stream to disconnect.
            while (!mTestFailed && mAutomatedTestRunner.isThreadEnabled() && !mSkipTest && (timeoutCount > 0) &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTED) {
                Thread.sleep(POLL_DURATION_MILLIS);
                timeoutCount--;
                if (timeoutCount == 0) {
                    mTestFailed = true;
                } else {
                    setStatusText("Plug detected by Java.\nCounting down to Oboe failure: " + timeoutCount);
                }
            }
            if (!mTestFailed) {
                int error = stream.getLastErrorCallbackResult();
                if (error != StreamConfiguration.ERROR_DISCONNECTED) {
                    log("onEerrorCallback error = " + error
                            + ", expected " + StreamConfiguration.ERROR_DISCONNECTED);
                    mTestFailed = true;
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
                appendFailedSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                appendFailedSummary(getConfigText(requestedConfig) + "\n");
                appendFailedSummary("Open failed!\n");
                mAutomatedTestRunner.incrementFailCount();
            } else {
                log("Result:");
                boolean passed = !mTestFailed;
                String resultText = requestPlugin ? "plugIN" : "UNplug";
                resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
                log(resultText);
                if (!passed) {
                    appendFailedSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                    appendFailedSummary("  " + actualConfigText + "\n");
                    appendFailedSummary("    " + resultText + "\n");
                    mAutomatedTestRunner.incrementFailCount();
                } else {
                    mAutomatedTestRunner.incrementPassCount();
                }
            }
        } else {
            log(TEXT_SKIP);
        }
        // Give hardware time to settle between tests.
        Thread.sleep(1000);
        mAutomatedTestRunner.incrementTestCount();
    }

    private void testConfiguration(boolean isInput, int performanceMode,
                                   int sharingMode, int sampleRate) throws InterruptedException {
        boolean requestPlugin = true; // plug IN
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate, requestPlugin);
        requestPlugin = false; // UNplug
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate, requestPlugin);
    }

    private void testConfiguration(boolean isInput, int performanceMode,
                                   int sharingMode) throws InterruptedException {
        final int sampleRate = 0;
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate);
    }

    private void testConfiguration(int performanceMode,
                                   int sharingMode) throws InterruptedException {
        testConfiguration(false, performanceMode, sharingMode);
        testConfiguration(true, performanceMode, sharingMode);
    }

    @Override
    public void runTest() {
        mPlugCount = 0;
        // Try several different configurations.
        try {
            testConfiguration(false, StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_EXCLUSIVE, 44100);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_EXCLUSIVE);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_SHARED);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_NONE,
                    StreamConfiguration.SHARING_MODE_SHARED);
        } catch (InterruptedException e) {
            log(e.getMessage());
            showErrorToast(e.getMessage());
        } finally {
            setInstructionsText("Test completed.");
            updateFailSkipButton(false);
        }
    }
}
