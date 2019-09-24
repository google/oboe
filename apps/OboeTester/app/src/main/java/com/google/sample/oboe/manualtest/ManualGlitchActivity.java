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
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.SeekBar;
import android.widget.TextView;

import java.io.IOException;

public class ManualGlitchActivity extends GlitchActivity {

    public static final String KEY_IN_PERF = "in_perf";
    public static final String KEY_OUT_PERF = "out_perf";
    public static final String VALUE_PERF_LOW_LATENCY = "lowlat";
    public static final String VALUE_PERF_POWERSAVE = "powersave";
    public static final String VALUE_PERF_NONE = "none";

    public static final String KEY_IN_SHARING = "in_sharing";
    public static final String KEY_OUT_SHARING = "out_sharing";
    public static final String VALUE_SHARING_EXCLUSIVE = "exclusive";
    public static final String VALUE_SHARING_SHARED = "shared";

    public static final String KEY_SAMPLE_RATE = "sample_rate";
    public static final int VALUE_DEFAULT_SAMPLE_RATE = 48000;

    public static final String KEY_IN_CHANNELS = "in_channels";
    public static final String KEY_OUT_CHANNELS = "out_channels";
    public static final int VALUE_DEFAULT_CHANNELS = 2;

    public static final String KEY_DURATION = "duration";
    public static final int VALUE_DEFAULT_DURATION = 10;

    public static final String KEY_BUFFER_BURSTS = "buffer_bursts";
    public static final int VALUE_DEFAULT_BUFFER_BURSTS = 2;

    private static final double DEFAULT_TOLERANCE = 0.1;

    private TextView mTextTolerance;
    private SeekBar mFaderTolerance;
    protected ExponentialTaper mTaperTolerance;
    private boolean mTestRunningByIntent;
    private Bundle mBundleFromIntent;

    private SeekBar.OnSeekBarChangeListener mToleranceListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            setToleranceProgress(progress);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };

    protected void setToleranceProgress(int progress) {
        double tolerance = mTaperTolerance.linearToExponential(
                ((double)progress) / FADER_PROGRESS_MAX);
        setTolerance((float)tolerance);
        mTextTolerance.setText("Tolerance = " + String.format("%5.3f", tolerance));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mBundleFromIntent = getIntent().getExtras();

        mTextTolerance = (TextView) findViewById(R.id.textTolerance);
        mFaderTolerance = (SeekBar) findViewById(R.id.faderTolerance);
        mTaperTolerance = new ExponentialTaper(0.0, 0.5, 100.0);
        int defaultProgress = (int) Math.round((mTaperTolerance.exponentialToLinear(
                DEFAULT_TOLERANCE) * FADER_PROGRESS_MAX));
        mFaderTolerance.setOnSeekBarChangeListener(mToleranceListener);
        mFaderTolerance.setProgress(defaultProgress);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_manual_glitches);
    }

    @Override
    public void onResume(){
        super.onResume();
        processBundleFromIntent();
    }

    @Override
    public void onNewIntent(Intent intent) {
        mBundleFromIntent = intent.getExtras();
    }

    private void processBundleFromIntent() {
        if (mBundleFromIntent == null) {
            return;
        }
        if (mTestRunningByIntent) {
            return;
        }

        mResultFileName = null;
        if (mBundleFromIntent.containsKey(KEY_FILE_NAME)) {
            mTestRunningByIntent = true;
            mResultFileName = mBundleFromIntent.getString(KEY_FILE_NAME);

            // Delay the test start to avoid race conditions.
            Handler handler = new Handler(Looper.getMainLooper()); // UI thread
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    startAutomaticTest();
                }
            }, 500); // TODO where is the race, close->open?

        }
    }

    private int getPerfFromText(String text) {
        if (VALUE_PERF_NONE.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_NONE;
        } else if (VALUE_PERF_POWERSAVE.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_POWER_SAVING;
        } else {
            return StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY;
        }
    }

    private int getSharingFromText(String text) {
        if (VALUE_SHARING_SHARED.equals(text)) {
            return StreamConfiguration.SHARING_MODE_SHARED;
        } else {
            return StreamConfiguration.SHARING_MODE_EXCLUSIVE;
        }
    }

    void configureStreamsFromBundle(Bundle bundle) {

        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();

        // Extract parameters from the bundle.
        String text = bundle.getString(KEY_IN_PERF, VALUE_PERF_LOW_LATENCY);
        int perfMode = getPerfFromText(text);
        requestedInConfig.setPerformanceMode(perfMode);

        text = bundle.getString(KEY_OUT_PERF, VALUE_PERF_LOW_LATENCY);
        perfMode = getPerfFromText(text);
        requestedOutConfig.setPerformanceMode(perfMode);

        text = bundle.getString(KEY_IN_SHARING, VALUE_SHARING_EXCLUSIVE);
        int sharingMode = getSharingFromText(text);
        requestedInConfig.setSharingMode(sharingMode);
        text = bundle.getString(KEY_OUT_SHARING, VALUE_SHARING_EXCLUSIVE);
        sharingMode = getSharingFromText(text);
        requestedOutConfig.setSharingMode(sharingMode);

        int sampleRate = bundle.getInt(KEY_SAMPLE_RATE, VALUE_DEFAULT_SAMPLE_RATE);
        requestedInConfig.setSampleRate(sampleRate);
        requestedOutConfig.setSampleRate(sampleRate);

        int inChannels = bundle.getInt(KEY_IN_CHANNELS, VALUE_DEFAULT_CHANNELS);
        requestedInConfig.setChannelCount(inChannels);
        int outChannels = bundle.getInt(KEY_OUT_CHANNELS, VALUE_DEFAULT_CHANNELS);
        requestedOutConfig.setChannelCount(outChannels);
    }

    public void startAudioTest() throws IOException {
        super.startAudioTest();
        setToleranceProgress(mFaderTolerance.getProgress());
    }

    void startAutomaticTest() {
        configureStreamsFromBundle(mBundleFromIntent);

        int durationSeconds = mBundleFromIntent.getInt(KEY_DURATION, VALUE_DEFAULT_DURATION);
        int numBursts = mBundleFromIntent.getInt(KEY_BUFFER_BURSTS, VALUE_DEFAULT_BUFFER_BURSTS);
        mBundleFromIntent = null;

        try {
            onStartAudioTest(null);
            int sizeFrames = mAudioOutTester.getCurrentAudioStream().getFramesPerBurst() * numBursts;
            mAudioOutTester.getCurrentAudioStream().setBufferSizeInFrames(sizeFrames);

            // Schedule the end of the test.
            Handler handler = new Handler(Looper.getMainLooper()); // UI thread
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    stopAutomaticTest();
                }
            }, durationSeconds * 1000);
        } catch (IOException e) {
            String report = "Open failed: " + e.getMessage();
            maybeWriteTestResult(report);
            mTestRunningByIntent = false;
        }

    }

    void stopAutomaticTest() {
        String report = getCommonTestReport() + mLastGlitchReport;
        onStopAudioTest(null);
        maybeWriteTestResult(report);
        mTestRunningByIntent = false;
    }

    // Only call from UI thread.
    @Override
    public void onTestFinished() {
        super.onTestFinished();
    }

}
