package com.mobileer.oboetester;

import java.io.IOException;

public class TapToToneTester {

    public static class TestResult {
        public float[] samples;
        public float[] filtered;
        public int frameRate;
        public TapLatencyAnalyser.TapLatencyEvent[] events;
    }

    private static final float MAX_TOUCH_LATENCY = 0.200f;
    private static final float MAX_OUTPUT_LATENCY = 0.600f;
    private static final float ANALYSIS_TIME_MARGIN = 0.250f;

    private static final float ANALYSIS_TIME_DELAY = MAX_OUTPUT_LATENCY;
    private static final float ANALYSIS_TIME_TOTAL = MAX_TOUCH_LATENCY + MAX_OUTPUT_LATENCY;
    private static final float ANALYSIS_TIME_MAX = ANALYSIS_TIME_TOTAL + ANALYSIS_TIME_MARGIN;
    private static final int ANALYSIS_SAMPLE_RATE = 48000; // need not match output rate

    private boolean mRecordEnabled = true;
    private AudioRecordThread mRecorder;
    private TapLatencyAnalyser mTapLatencyAnalyser;

    public TapToToneTester() {
        if (mRecordEnabled) {
            mRecorder = new AudioRecordThread(ANALYSIS_SAMPLE_RATE,
                    1,
                    (int) (ANALYSIS_TIME_MAX * ANALYSIS_SAMPLE_RATE));
        }
        mTapLatencyAnalyser = new TapLatencyAnalyser();
    }

    public void start() throws IOException {
        if (mRecordEnabled) {
            mRecorder.startAudio();
        }
    }

    public void stop() {
        if (mRecordEnabled) {
            mRecorder.stopAudio();
        }
    }

    public void scheduleTaskWhenDone(Runnable task) {
        if (mRecordEnabled) {
            // schedule an analysis to start in the near future
            int numSamples = (int) (mRecorder.getSampleRate() * ANALYSIS_TIME_DELAY);
            mRecorder.scheduleTask(numSamples, task);
        }
    }

    public TestResult analyzeCapturedAudio() {
        if (!mRecordEnabled) return null;
        int numSamples = (int) (mRecorder.getSampleRate() * ANALYSIS_TIME_TOTAL);
        float[] buffer = new float[numSamples];
        mRecorder.setCaptureEnabled(false); // TODO wait for it to settle
        int numRead = mRecorder.readMostRecent(buffer);

        TestResult result = new TestResult();
        result.samples = buffer;
        result.frameRate = mRecorder.getSampleRate();
        result.events = mTapLatencyAnalyser.analyze(buffer, 0, numRead);
        result.filtered = mTapLatencyAnalyser.getFilteredBuffer();
        mRecorder.setCaptureEnabled(true);
        return result;
    }

}
