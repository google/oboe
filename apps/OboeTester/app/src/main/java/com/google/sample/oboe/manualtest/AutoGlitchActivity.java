package com.google.sample.oboe.manualtest;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import java.util.Date;

public class AutoGlitchActivity extends GlitchActivity implements Runnable {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    private static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Run time for each test.
    private static final String TEXT_SKIP = "SKIP";
    public static final String TEXT_PASS = "PASS";
    public static final String TEXT_FAIL = "FAIL !!!!";

    private TextView mAutoTextView;
    private Button mButtonShare;

    private Thread mAutoThread;
    private volatile boolean mThreadEnabled = false;
    int mTestCount = 0;
    private int mDurationSeconds = DEFAULT_DURATION_SECONDS;
    private int mGapMillis = DEFAULT_GAP_MILLIS;
    private StringBuffer mFailedSummary;
    private Spinner mDurationSpinner;

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

        mButtonShare = (Button) findViewById(R.id.button_share);
        mButtonShare.setEnabled(false);

        mAutoTextView = (TextView) findViewById(R.id.text_auto_result);
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

    public void startAudioTest() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mButtonShare.setEnabled(false);
                // Keep screen on because the test takes a while and sleep might cause glitches.
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
        mThreadEnabled = true;
        mAutoThread = new Thread(this);
        mAutoThread.start();
    }

    // Only call from UI thread.
    @Override
    public void onTestFinished() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mButtonShare.setEnabled(true);
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

    // Parameters to be tested.
    private static final int[] PERF_MODES = {
        StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
        StreamConfiguration.PERFORMANCE_MODE_NONE
    };

    private static final int[] SHARING_MODES = {
        StreamConfiguration.SHARING_MODE_EXCLUSIVE,
        StreamConfiguration.SHARING_MODE_SHARED
    };

    private static final int[] CHANNEL_COUNTS = {1, 2};

    private static final int[] SAMPLE_RATES = {48000, 44100};

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

    private void testConfiguration(StreamConfiguration requestedConfig,
                                    StreamConfiguration actualConfig) throws InterruptedException {
        int perfMode = requestedConfig.getPerformanceMode();
        int sharingMode = requestedConfig.getSharingMode();
        if ((perfMode != StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY)
                && (sharingMode == StreamConfiguration.SHARING_MODE_EXCLUSIVE)) {
            return;
        }
        log("------------------ #" + mTestCount++);
        String configText = getConfigText(requestedConfig);
        log(configText);
        super.startAudioTest(); // this will fill in actualConfig

        // Set output size to a level that will avoit glitches.
        int sizeFrames = mAudioOutTester.getCurrentAudioStream().getBufferCapacityInFrames() / 2;
        mAudioOutTester.getCurrentAudioStream().setBufferSizeInFrames(sizeFrames);

        // The test would only be valid if we got the configuration we requested.
        boolean valid = true;
        int actualPerfMode = actualConfig.getPerformanceMode();
        if (actualPerfMode != perfMode) {
            log("actual perf mode = "
                    + StreamConfiguration.convertPerformanceModeToText(actualPerfMode));
            valid = false;
        }
        int actualSharingMode = actualConfig.getSharingMode();
        if (actualSharingMode != sharingMode) {
            log("actual sharing mode = "
                    + StreamConfiguration.convertSharingModeToText(actualSharingMode));
            valid = false;
        }

        if (valid) {
            Thread.sleep(mDurationSeconds * 1000);
        }

        int inXRuns = mAudioInputTester.getCurrentAudioStream().getXRunCount();
        int outXRuns = mAudioOutTester.getCurrentAudioStream().getXRunCount();

        super.stopAudioTest();
        if (valid) {
            boolean passed = (getMaxSecondsWithNoGlitch()
                    > (mDurationSeconds - SETUP_TIME_SECONDS));
            String resultText = "#gl = " + getLastGlitchCount()
                    + ", time no gl = " + getMaxSecondsWithNoGlitch()
                    + ", xruns = " + inXRuns + "/" + outXRuns;
            log(resultText + ", " + (passed ? TEXT_PASS : TEXT_FAIL)
            );
            if (!passed) {
                mFailedSummary.append("  ");
                mFailedSummary.append(configText);
                mFailedSummary.append("\n");
                mFailedSummary.append("    ");
                mFailedSummary.append(resultText);
                mFailedSummary.append("\n");
            }
        } else {
            log(TEXT_SKIP);
        }
        // Give hardware time to settle between tests.
        Thread.sleep(mGapMillis);
    }

    private void testCombinations(StreamConfiguration requestedConfig,
                                    StreamConfiguration actualConfig) throws InterruptedException {
         for (int perfMode : PERF_MODES) {
            for (int sharingMode : SHARING_MODES) {
                for (int channelCount : CHANNEL_COUNTS) {
                    if (!mThreadEnabled) return;

                    requestedConfig.setPerformanceMode(perfMode);
                    requestedConfig.setSharingMode(sharingMode);
                    requestedConfig.setChannelCount(channelCount);

                    testConfiguration(requestedConfig, actualConfig);
                }
            }
        }
    }

    @Override
    public void run() {
        log("=== STARTED at " + new Date());
        log(Build.MANUFACTURER + " " + Build.PRODUCT);
        log(Build.DISPLAY);
        mFailedSummary = new StringBuffer();
        mTestCount = 0;
        try {
            // Configure settings
            StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
            StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
            StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
            StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;

            for (int sampleRate : SAMPLE_RATES) {
                // Sample rates must match.
                requestedOutConfig.setSampleRate(sampleRate);
                requestedInConfig.setSampleRate(sampleRate);

                // Use optimal input configuration while testing for output glitches.
                requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);
                requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
                log("===========================");
                log(getConfigText(requestedInConfig));
                testCombinations(requestedOutConfig, actualOutConfig);

                // Use optimal output configuration while testing for input glitches.
                requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
                requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_EXCLUSIVE);
                log("===========================");
                log(getConfigText(requestedOutConfig));
                testCombinations(requestedInConfig, actualInConfig);
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        } finally {
            super.stopAudioTest();
            log("\n==== SUMMARY ========");
            if (mFailedSummary.length() > 0) {
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
