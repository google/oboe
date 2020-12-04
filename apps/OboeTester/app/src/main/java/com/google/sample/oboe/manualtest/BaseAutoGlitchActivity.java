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

import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Spinner;

import java.io.IOException;

public class BaseAutoGlitchActivity extends GlitchActivity {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    protected static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Run time for each test.
    private static final String TEXT_SKIP = "SKIP";
    public static final String TEXT_PASS = "PASS";
    public static final String TEXT_FAIL = "FAIL !!!!";

    protected int mDurationSeconds = DEFAULT_DURATION_SECONDS;
    private int mGapMillis = DEFAULT_GAP_MILLIS;

    protected AutomatedTestRunner mAutomatedTestRunner;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAutomatedTestRunner = findViewById(R.id.auto_test_runner);
        mAutomatedTestRunner.setActivity(this);
    }

    protected void log(String text) {
        mAutomatedTestRunner.log(text);
    }

    private void appendSummary(String text) {
        mAutomatedTestRunner.appendSummary(text);
    }


    // This should only be called from UI events such as onStop or a button press.
    @Override
    public void onStopTest() {
        mAutomatedTestRunner.stopTest();
    }

    @Override
    public void stopAudioTest() {
        super.stopAudioTest();
        mAutomatedTestRunner.stopTest();
    }

    protected String getConfigText(StreamConfiguration config) {
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "IN")
                + ", SR = " + config.getSampleRate()
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", ch = " + config.getChannelCount();
    }

    // Run test based on the requested input/output configurations.
    protected void testConfigurations() throws InterruptedException {
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;

        log("========================== #" + mAutomatedTestRunner.getTestCount());
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
        if (!openFailed && actualInConfig.getSharingMode() != requestedInConfig.getSharingMode()
                && actualOutConfig.getSharingMode() != requestedOutConfig.getSharingMode()) {
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
                appendSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                appendSummary(getConfigText(requestedInConfig) + "\n");
                appendSummary(getConfigText(requestedOutConfig) + "\n");
                appendSummary("Open failed!\n");
                mAutomatedTestRunner.incrementFailCount();
            } else {
                log("Result:");
                boolean passed = didTestPass();

                String resultText = getShortReport();
                resultText += ", xruns = " + inXRuns + "/" + outXRuns;
                resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
                log(resultText);
                if (!passed) {
                    appendSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                    appendSummary("  " + getConfigText(actualInConfig) + "\n");
                    appendSummary("    " + resultText + "\n");
                    mAutomatedTestRunner.incrementFailCount();
                } else {
                    mAutomatedTestRunner.incrementPassCount();
                }
            }
        } else {
            log(TEXT_SKIP);
        }
        // Give hardware time to settle between tests.
        Thread.sleep(mGapMillis);
        mAutomatedTestRunner.incrementTestCount();
    }

    public boolean didTestPass() {
        return getMaxSecondsWithNoGlitch()
                > (mDurationSeconds - SETUP_TIME_SECONDS);
    }

}
