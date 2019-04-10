package com.google.sample.oboe.manualtest;

import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.widget.TextView;

import java.util.Date;

public class AutoGlitchActivity extends GlitchActivity implements Runnable {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    private static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Run time for each test.

    private Thread mAutoThread;
    private TextView mAutoTextView;
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
        mThreadEnabled = true;
        mAutoThread = new Thread(this);
        mAutoThread.start();
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

    private void testCombinations(StreamConfiguration requestedConfig,
                                  StreamConfiguration actualConfig,
                                  String label) throws InterruptedException {
        int testCount = 0;
        for (int perfMode : PERF_MODES) {
            requestedConfig.setPerformanceMode(perfMode);
            for (int sharingMode : SHARING_MODES) {
                requestedConfig.setSharingMode(sharingMode);
                for (int channelCount : CHANNEL_COUNTS) {
                    if (!mThreadEnabled) break;
                    requestedConfig.setChannelCount(channelCount);

                    log("------------------ #" + testCount++ + ", " + label);
                    String text = " Perf = " + StreamConfiguration.convertPerformanceModeToText(perfMode)
                            + ", " + StreamConfiguration.convertSharingModeToText(sharingMode)
                            + ", ch = " + channelCount;
                    Log.d(TAG, text);
                    log(text);
                    super.startAudioTest(); // this will fill in actualConfig

                    // decide whether the test would be valid
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
                    super.stopAudioTest();
                    if (valid) {
                        boolean passed = (getMaxSecondsWithNoGlitch()
                                > (mDurationSeconds - SETUP_TIME_SECONDS));
                        log("glitch = " + getLastGlitchCount()
                                + ", max no glitches = " + getMaxSecondsWithNoGlitch()
                                + ", " + (passed ? "PASS" : "FAIL !!!!")
                        );
                    } else {
                        log("N/A");
                    }
                    // Give hardware time to settle between tests.
                    Thread.sleep(mGapMillis);
                }
            }
        }
    }

    @Override
    public void run() {
        log("=== STARTED at " + new Date());
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
                requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
                requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_EXCLUSIVE);
                String label = "OUTPUT, " + sampleRate;
                testCombinations(requestedOutConfig, actualOutConfig, label);

                // Use optimal output configuration while testing for input glitches.
                requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
                requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_EXCLUSIVE);
                label = "INPUT, " + sampleRate;
                testCombinations(requestedInConfig, actualInConfig, label);
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        } finally {
            super.stopAudioTest();

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
