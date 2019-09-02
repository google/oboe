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
import android.os.Handler;
import android.os.Looper;

import java.util.Date;

public class ManualGlitchActivity extends GlitchActivity {

    private boolean mTestRunningByIntent;
    private Bundle mBundleFromIntent;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mBundleFromIntent = getIntent().getExtras();
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
        if ("none".equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_NONE;
        } else if ("powersave".equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_POWER_SAVING;
        } else {
            return StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY;
        }
    }
    private int getSharingFromText(String text) {
        if ("shared".equals(text)) {
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
        String text = bundle.getString("in_perf", "lowlat");
        int perfMode = getPerfFromText(text);
        requestedInConfig.setPerformanceMode(perfMode);

        text = bundle.getString("out_perf", "lowlat");
        perfMode = getPerfFromText(text);
        requestedOutConfig.setPerformanceMode(perfMode);

        text = bundle.getString("in_sharing", "exclusive");
        int sharingMode = getSharingFromText(text);
        requestedInConfig.setSharingMode(sharingMode);
        text = bundle.getString("out_sharing", "exclusive");
        sharingMode = getSharingFromText(text);
        requestedOutConfig.setSharingMode(sharingMode);

        int sampleRate = bundle.getInt("sample_rate", 48000);
        requestedInConfig.setSampleRate(sampleRate);
        requestedOutConfig.setSampleRate(sampleRate);

        int inChannels = bundle.getInt("in_channels", 2);
        requestedInConfig.setChannelCount(inChannels);
        int outChannels = bundle.getInt("out_channels", 2);
        requestedOutConfig.setChannelCount(outChannels);
    }

    void startAutomaticTest() {
        configureStreamsFromBundle(mBundleFromIntent);

        int durationSeconds = mBundleFromIntent.getInt("duration", 10);
        int numBursts = mBundleFromIntent.getInt("buffer_bursts", 2);
        mBundleFromIntent = null;

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
    }

    private String generateReport() {
        StringBuffer report = new StringBuffer();

        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        report.append(actualInConfig.dump());
        report.append(actualOutConfig.dump());

        int inXRuns = mAudioInputTester.getCurrentAudioStream().getXRunCount();
        report.append(String.format("in.xruns = %d\n", inXRuns));
        int outXRuns = mAudioOutTester.getCurrentAudioStream().getXRunCount();
        report.append(String.format("out.xruns = %d\n", outXRuns));
        report.append(mLastGlitchReport);

        return report.toString();
    }

    void stopAutomaticTest() {
        String report = generateReport();
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
