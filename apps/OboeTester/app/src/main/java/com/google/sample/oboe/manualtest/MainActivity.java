/*
 * Copyright 2017 The Android Open Source Project
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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TextView;

/**
 * Select various Audio tests.
 */

public class MainActivity extends Activity {

    static {
        // Must match name in CMakeLists.txt
        System.loadLibrary("oboetester");
    }

    private static final String KEY_TEST_NAME = "test";

    private Spinner mModeSpinner;
    private TextView mCallbackSizeTextView;
    protected TextView mDeviceView;
    private TextView mVersionTextView;
    private TextView mBuildTextView;
    private Bundle mBundleFromIntent;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mVersionTextView = (TextView) findViewById(R.id.versionText);
        mCallbackSizeTextView = (TextView) findViewById(R.id.callbackSize);

        mDeviceView = (TextView) findViewById(R.id.deviceView);
        updateNativeAudioUI();

        // Set mode, eg. MODE_IN_COMMUNICATION
        mModeSpinner = (Spinner) findViewById(R.id.spinnerAudioMode);
        mModeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                long mode = mModeSpinner.getSelectedItemId();
                AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
                myAudioMgr.setMode((int)mode);
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });

        try {
            PackageInfo pinfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            int versionCode = pinfo.versionCode;
            String versionName = pinfo.versionName;
            mVersionTextView.setText("V# = " + versionCode + ", name = " + versionName);
        } catch (PackageManager.NameNotFoundException e) {
            mVersionTextView.setText(e.getMessage());
        }

        mBuildTextView = (TextView) findViewById(R.id.text_build_info);
        mBuildTextView.setText(Build.DISPLAY);

        saveIntentBundleForLaterProcessing(getIntent());
    }

    @Override
    public void onNewIntent(Intent intent) {
        saveIntentBundleForLaterProcessing(intent);
    }

    // This will get processed during onResume.
    private void saveIntentBundleForLaterProcessing(Intent intent) {
        mBundleFromIntent = intent.getExtras();
    }

    private void processBundleFromIntent() {
        if (mBundleFromIntent == null) {
            return;
        }

        if (mBundleFromIntent.containsKey(KEY_TEST_NAME)) {
            String testName = mBundleFromIntent.getString(KEY_TEST_NAME);
            if ("latency".equals(testName)) {
                Intent intent = new Intent(this, RoundTripLatencyActivity.class);
                intent.putExtras(mBundleFromIntent);
                startActivity(intent);
            }
        }
        mBundleFromIntent = null;
    }

    @Override
    public void onResume(){
        super.onResume();
        processBundleFromIntent();
    }

    private void updateNativeAudioUI() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        String audioManagerSampleRate = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        String audioManagerFramesPerBurst = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        mDeviceView.setText("Java AudioManager: rate = " + audioManagerSampleRate +
                ", burst = " + audioManagerFramesPerBurst);
    }

    public void onLaunchTestOutput(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, TestOutputActivity.class);
        startActivity(intent);
    }

    public void onLaunchTestInput(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, TestInputActivity.class);
        startActivity(intent);
    }

    public void onLaunchTapToTone(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, TapToToneActivity.class);
        startActivity(intent);
    }

    public void onLaunchRecorder(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, RecorderActivity.class);
        startActivity(intent);
    }

    public void onLaunchEcho(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, EchoActivity.class);
        startActivity(intent);
    }

    public void onLaunchRoundTripLatency(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, RoundTripLatencyActivity.class);
        startActivity(intent);
    }

    public void onLaunchManualGlitchTest(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, ManualGlitchActivity.class);
        startActivity(intent);
    }

    public void onLaunchAutoGlitchTest(View view) {
        updateCallbackSize();
        Intent intent = new Intent(this, AutoGlitchActivity.class);
        startActivity(intent);
    }

    public void onUseCallbackClicked(View view) {
        CheckBox checkBox = (CheckBox) view;
        OboeAudioStream.setUseCallback(checkBox.isChecked());
    }

    private void updateCallbackSize() {
        CharSequence chars = mCallbackSizeTextView.getText();
        String text = chars.toString();
        int callbackSize = Integer.parseInt(text);
        OboeAudioStream.setCallbackSize(callbackSize);
    }

    public void onSetSpeakerphoneOn(View view) {
        CheckBox checkBox = (CheckBox) view;
        boolean enabled = checkBox.isChecked();
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        myAudioMgr.setSpeakerphoneOn(enabled);
    }

}
