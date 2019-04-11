package com.google.sample.oboe.manualtest;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

public class AutoGlitchActivity extends GlitchActivity implements Runnable {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    private static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Run time for each test.

    private Thread mAutoThread;
    private TextView mAutoTextView;
    private Button mButtonShare;
    private volatile boolean mThreadEnabled = false;
    private int mDurationSeconds = DEFAULT_DURATION_SECONDS;
    private int mGapMillis = DEFAULT_GAP_MILLIS;

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
    }

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
            }
        });
        mThreadEnabled = true;
        mAutoThread = new Thread(this);
        mAutoThread.start();
    }

    @Override
    public void onTestFinished() {
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

    private static final int[] PERF_MODES = {
            StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
            StreamConfiguration.PERFORMANCE_MODE_NONE,
    };

    private static final int[] SHARING_MODES = {
            StreamConfiguration.SHARING_MODE_EXCLUSIVE,
            StreamConfiguration.SHARING_MODE_SHARED
    };

    private static final int[] CHANNEL_COUNTS = {1, 2};

    private static final int[] SAMPLE_RATES = {48000, 44100};

    private String getTimestampString() {
        DateFormat df = new SimpleDateFormat("yyyyMMdd-HHmmss");
        Date now = Calendar.getInstance().getTime();
        return df.format(now);
    }

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
        return "" + ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "IN")
                + ", SR = " + config.getSampleRate()
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", ch = " + config.getChannelCount();
    }

    private String testCombinations(StreamConfiguration requestedConfig,
                                    StreamConfiguration actualConfig) throws InterruptedException {
        StringBuffer summary = new StringBuffer();
        int testCount = 0;
        for (int perfMode : PERF_MODES) {
            requestedConfig.setPerformanceMode(perfMode);
            for (int sharingMode : SHARING_MODES) {
                if (perfMode != StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY
                    && sharingMode == StreamConfiguration.SHARING_MODE_EXCLUSIVE) {
                    break; // not possible
                }
                requestedConfig.setSharingMode(sharingMode);
                for (int channelCount : CHANNEL_COUNTS) {
                    if (!mThreadEnabled) break;
                    requestedConfig.setChannelCount(channelCount);

                    log("------------------ #" + testCount++);
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
                        log(resultText + ", " + (passed ? "PASS" : "FAIL !!!!")
                        );
                        if (!passed) {
                            summary.append("  ");
                            summary.append(configText);
                            summary.append("\n");
                            summary.append("    ");
                            summary.append(resultText);
                            summary.append("\n");
                        }
                    } else {
                        log("N/A");
                    }
                    // Give hardware time to settle between tests.
                    Thread.sleep(mGapMillis);
                }
            }
        }
        return summary.toString();
    }

    @Override
    public void run() {
        log("=== STARTED at " + new Date());
        log(Build.MANUFACTURER + " " + Build.PRODUCT);
        log(Build.DISPLAY);
        StringBuffer summary = new StringBuffer();
        summary.append("These tests failed:\n");
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
                summary.append(testCombinations(requestedOutConfig, actualOutConfig));

                // Use optimal output configuration while testing for input glitches.
                requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
                requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_EXCLUSIVE);
                log("===========================");
                log(getConfigText(requestedOutConfig));
                summary.append(testCombinations(requestedInConfig, actualInConfig));
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        } finally {
            super.stopAudioTest();

            log("=== SUMMARY");
            log(summary.toString());
            log("=== FINISHED at " + new Date());
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    onTestFinished();
                }
            });
        }
    }

}
