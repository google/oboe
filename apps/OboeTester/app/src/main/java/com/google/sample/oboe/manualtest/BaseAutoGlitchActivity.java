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

import java.io.IOException;

public class BaseAutoGlitchActivity extends GlitchActivity {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    protected static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Idle time between each test.
    private static final String TEXT_SKIP = "SKIP";
    public static final String TEXT_PASS = "PASS";
    public static final String TEXT_FAIL = "FAIL !!!!";

    protected int mDurationSeconds = DEFAULT_DURATION_SECONDS;
    protected int mGapMillis = DEFAULT_GAP_MILLIS;

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

    protected void appendFailedSummary(String text) {
        mAutomatedTestRunner.appendFailedSummary(text);
    }

    protected void appendSummary(String text) {
        mAutomatedTestRunner.appendSummary(text);
    }

    @Override
    public void onStopTest() {
        mAutomatedTestRunner.stopTest();
    }

    protected String getConfigText(StreamConfiguration config) {
        int channel = (config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT)
                ? getOutputChannel() : getInputChannel();
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "INP")
                + (config.isMMap() ? "-M" : "-L")
                + ", ID = " + String.format("%2d", config.getDeviceId())
                + ", SR = " + String.format("%5d", config.getSampleRate())
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", ch = " + config.getChannelCount() + "[" + channel + "]";
    }

    public final static int TEST_RESULT_FAILED = -2;
    public final static int TEST_RESULT_WARNING = -1;
    public final static int TEST_RESULT_SKIPPED = 0;
    public final static int TEST_RESULT_PASSED = 1;

    // Run test based on the requested input/output configurations.
    protected int testConfigurations() throws InterruptedException {
        int result = TEST_RESULT_SKIPPED;
        mAutomatedTestRunner.incrementTestCount();
        if ((getSingleTestIndex() >= 0) && (mAutomatedTestRunner.getTestCount() != getSingleTestIndex())) {
            return result;
        }

        log("========================== #" + mAutomatedTestRunner.getTestCount());

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;

        log("Requested:");
        log("  " + getConfigText(requestedInConfig));
        log("  " + getConfigText(requestedOutConfig));

        String reason = "";
        boolean openFailed = false;
        try {
            openAudio(); // this will fill in actualConfig
            log("Actual:");
            log("  " + getConfigText(actualInConfig));
            log("  " + getConfigText(actualOutConfig));
            // Set output size to a level that will avoid glitches.
            AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
            int sizeFrames = stream.getBufferCapacityInFrames() / 2;
            stream.setBufferSizeInFrames(sizeFrames);
        } catch (Exception e) {
            openFailed = true;
            log(e.getMessage());
            reason = e.getMessage();
        }

        // The test would only be worth running if we got the configuration we requested on input or output.
        String skipReason = shouldTestBeSkipped();
        boolean skipped = skipReason.length() > 0;
        boolean valid = !openFailed && !skipped;
        boolean startFailed = false;
        if (valid) {
            try {
                startAudioTest();
            } catch (IOException e) {
                e.printStackTrace();
                valid = false;
                startFailed = true;
                log(e.getMessage());
                reason = e.getMessage();
            }
        }
        mAutomatedTestRunner.flushLog();

        if (valid) {
            // Check for early return until we reach full duration.
            long now = System.currentTimeMillis();
            long startedAt = now;
            long endTime = System.currentTimeMillis() + (mDurationSeconds * 1000);
            boolean finishedEarly = false;
            while (now < endTime && !finishedEarly) {
                Thread.sleep(100); // Let test run.
                now = System.currentTimeMillis();
                finishedEarly = isFinishedEarly();
                if (finishedEarly) {
                    log("Finished early after " + (now - startedAt) + " msec.");
                }
            }
        }
        int inXRuns = 0;
        int outXRuns = 0;

        if (!openFailed) {
            // get xRuns before closing the streams.
            inXRuns = mAudioInputTester.getCurrentAudioStream().getXRunCount();
            outXRuns = mAudioOutTester.getCurrentAudioStream().getXRunCount();

            super.stopAudioTest();
        }

        if (openFailed || startFailed) {
            appendFailedSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
            appendFailedSummary(getConfigText(requestedInConfig) + "\n");
            appendFailedSummary(getConfigText(requestedOutConfig) + "\n");
            appendFailedSummary(reason + "\n");
            mAutomatedTestRunner.incrementFailCount();
        } else if (skipped) {
            log(TEXT_SKIP + " - " + skipReason);
        } else {
            log("Result:");
            reason += didTestFail();
            boolean passed = reason.length() == 0;

            String resultText = getShortReport();
            resultText += ", xruns = " + inXRuns + "/" + outXRuns;
            resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
            resultText += reason;
            log("  " + resultText);
            if (!passed) {
                appendFailedSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                appendFailedSummary("  " + getConfigText(actualInConfig) + "\n");
                appendFailedSummary("  " + getConfigText(actualOutConfig) + "\n");
                appendFailedSummary("    " + resultText + "\n");
                mAutomatedTestRunner.incrementFailCount();
                result = TEST_RESULT_FAILED;
            } else {
                mAutomatedTestRunner.incrementPassCount();
                result = TEST_RESULT_PASSED;
            }
        }
        mAutomatedTestRunner.flushLog();

        // Give hardware time to settle between tests.
        Thread.sleep(mGapMillis);
        return result;
    }

    protected boolean isFinishedEarly() {
        return false;
    }

    protected String shouldTestBeSkipped() {
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        // No point running the test if we don't get the sharing mode we requested.
        if (actualInConfig.getSharingMode() != requestedInConfig.getSharingMode()
                || actualOutConfig.getSharingMode() != requestedOutConfig.getSharingMode()) {
            log("Did not get requested sharing mode.");
            why += "share";
        }
        // We don't skip based on performance mode because if you request LOW_LATENCY you might
        // get a smaller burst than if you request NONE.
        return why;
    }

    public String didTestFail() {
        String why = "";
        if (getMaxSecondsWithNoGlitch() <= (mDurationSeconds - SETUP_TIME_SECONDS)) {
            why += ", glitch";
        }
        return why;
    }

}
