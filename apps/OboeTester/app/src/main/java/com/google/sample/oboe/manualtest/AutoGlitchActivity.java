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

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.IOException;
import java.util.Date;

public class AutoGlitchActivity extends GlitchActivity implements Runnable {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    private static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Run time for each test.
    private static final String TEXT_SKIP = "SKIP";
    public static final String TEXT_PASS = "PASS";
    public static final String TEXT_FAIL = "FAIL !!!!";

    private TextView mAutoTextView;

    private Thread mAutoThread;
    private volatile boolean mThreadEnabled = false;
    int mTestCount = 0;
    private int mDurationSeconds = DEFAULT_DURATION_SECONDS;
    private int mGapMillis = DEFAULT_GAP_MILLIS;
    private StringBuffer mFailedSummary;
    private int mPassCount = 0;
    private int mFailCount = 0;
    private Spinner mDurationSpinner;

    // Test with these configurations.
    private static final int[] PERFORMANCE_MODES = {
            StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
            StreamConfiguration.PERFORMANCE_MODE_NONE
    };
    private static final int[] SAMPLE_RATES = { 48000, 44100, 16000 };

    private class DurationSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            String text = parent.getItemAtPosition(pos).toString();
            mDurationSeconds = Integer.parseInt(text);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mDurationSeconds = DEFAULT_DURATION_SECONDS;
        }
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_auto_glitches);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAutoTextView = (TextView) findViewById(R.id.text_log);
        mAutoTextView.setMovementMethod(new ScrollingMovementMethod());

        mDurationSpinner = (Spinner) findViewById(R.id.spinner_glitch_duration);
        mDurationSpinner.setOnItemSelectedListener(new DurationSpinnerListener());
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
    private void logClear() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAutoTextView.setText("");
            }
        });
    }

    public void startAudioTest() {
        mThreadEnabled = true;
        mAutoThread = new Thread(this);
        mAutoThread.start();
    }

    // Only call from UI thread.
    @Override
    public void onTestFinished() {
        super.onTestFinished();
    }

    public void stopAudioTest() {
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

    // Share text from log via GMail, Drive or other method.
    public void onShareResult(View view) {
        Intent sharingIntent = new Intent(android.content.Intent.ACTION_SEND);
        sharingIntent.setType("text/plain");

        String subjectText = "OboeTester AutoGlitch result " + getTimestampString();
        sharingIntent.putExtra(android.content.Intent.EXTRA_SUBJECT, subjectText);

        String shareBody = mAutoTextView.getText().toString();
        sharingIntent.putExtra(android.content.Intent.EXTRA_TEXT, shareBody);

        startActivity(Intent.createChooser(sharingIntent, "Share using:"));
    }

    private String getConfigText(StreamConfiguration config) {
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "IN")
                + ", SR = " + config.getSampleRate()
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                        config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", ch = " + config.getChannelCount();
    }

    private void testConfiguration(int perfMode,
                                   int sharingMode,
                                   int sampleRate,
                                   int inChannels,
                                   int outChannels) throws InterruptedException {

        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();

        requestedInConfig.setPerformanceMode(perfMode);
        requestedOutConfig.setPerformanceMode(perfMode);

        requestedInConfig.setSharingMode(sharingMode);
        requestedOutConfig.setSharingMode(sharingMode);

        requestedInConfig.setSampleRate(sampleRate);
        requestedOutConfig.setSampleRate(sampleRate);

        requestedInConfig.setChannelCount(inChannels);
        requestedOutConfig.setChannelCount(outChannels);

        log("========================== #" + mTestCount);
        log("Requested:");
        log(getConfigText(requestedInConfig));
        log(getConfigText(requestedOutConfig));

        // Give previous stream time to close and release resources. Avoid race conditions.
        Thread.sleep(1000);
        boolean openFailed = false;
        try {
            super.startAudioTest(); // this will fill in actualConfig
            log("Actual:");
            log(getConfigText(actualInConfig));
            log(getConfigText(actualOutConfig));
            // Set output size to a level that will avoid glitches.
            AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
            int sizeFrames = stream.getBufferCapacityInFrames() / 2;
            stream.setBufferSizeInFrames(sizeFrames);
        } catch (IOException e) {
            openFailed = true;
            log(e.getMessage());
        }

        // The test would only be worth running if we got the configuration we requested on input or output.
        boolean valid = true;
        // No point running the test if we don't get the sharing mode we requested.
        if (!openFailed && actualInConfig.getSharingMode() != sharingMode
                && actualOutConfig.getSharingMode() != sharingMode) {
            log("did not get requested sharing mode");
            valid = false;
        }
        // We don't skip based on performance mode because if you request LOW_LATENCY you might
        // get a smaller burst than if you request NONE.

        if (!openFailed && valid) {
            Thread.sleep(mDurationSeconds * 1000);
        }
        int inXRuns = 0;
        int outXRuns = 0;

        if (!openFailed) {
            // get xRuns before closing the streams.
            inXRuns = mAudioInputTester.getCurrentAudioStream().getXRunCount();
            outXRuns = mAudioOutTester.getCurrentAudioStream().getXRunCount();

            super.stopAudioTest();
        }

        if (valid) {
            if (openFailed) {
                mFailedSummary.append("------ #" + mTestCount);
                mFailedSummary.append("\n");
                mFailedSummary.append(getConfigText(requestedInConfig));
                mFailedSummary.append("\n");
                mFailedSummary.append(getConfigText(requestedOutConfig));
                mFailedSummary.append("\n");
                mFailedSummary.append("Open failed!\n");
                mFailCount++;
            } else {
                log("Result:");
                boolean passed = (getMaxSecondsWithNoGlitch()
                        > (mDurationSeconds - SETUP_TIME_SECONDS));
                String resultText = getShortReport();
                resultText += ", xruns = " + inXRuns + "/" + outXRuns;
                resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
                log(resultText);
                if (!passed) {
                    mFailedSummary.append("------ #" + mTestCount);
                    mFailedSummary.append("\n");
                    mFailedSummary.append("  ");
                    mFailedSummary.append(getConfigText(actualInConfig));
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
        Thread.sleep(mGapMillis);
        mTestCount++;
    }

    private void testConfiguration(int performanceMode,
                                   int sharingMode,
                                   int sampleRate) throws InterruptedException {
        testConfiguration(performanceMode,
                sharingMode,
                sampleRate, 1, 2);
        testConfiguration(performanceMode,
                sharingMode,
                sampleRate, 2, 1);
    }


    @Override
    public void run() {
        logClear();
        log("=== STARTED at " + new Date());
        log(Build.MANUFACTURER + " " + Build.PRODUCT);
        log(Build.DISPLAY);
        mFailedSummary = new StringBuffer();
        mTestCount = 0;
        mPassCount = 0;
        mFailCount = 0;
        try {
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_EXCLUSIVE,
                    0);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_SHARED,
                    0);

            for (int perfMode : PERFORMANCE_MODES) {
                for (int sampleRate : SAMPLE_RATES) {
                    testConfiguration(perfMode,
                            StreamConfiguration.SHARING_MODE_SHARED,
                            sampleRate);
                }
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        } finally {
            super.stopAudioTest();
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
        }
    }

}
