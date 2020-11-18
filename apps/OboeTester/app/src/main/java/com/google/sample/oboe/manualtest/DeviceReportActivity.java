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

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.MicrophoneInfo;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.widget.TextView;

import com.google.sample.audio_device.AudioDeviceInfoConverter;

import java.io.IOException;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;

/**
 * Guide the user through a series of tests plugging in and unplugging a headset.
 * Print a summary at the end of any failures.
 */
public class DeviceReportActivity extends Activity {

    class MyAudioDeviceCallback extends AudioDeviceCallback {
        private HashMap<Integer, AudioDeviceInfo> mDevices
                = new HashMap<Integer, AudioDeviceInfo>();

        @Override
        public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
            for (AudioDeviceInfo info : addedDevices) {
                mDevices.put(info.getId(), info);
            }
            reportDeviceInfo(mDevices.values());
        }

        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            for (AudioDeviceInfo info : removedDevices) {
                mDevices.remove(info.getId());
            }
            reportDeviceInfo(mDevices.values());
        }
    }

    MyAudioDeviceCallback mDeviceCallback = new MyAudioDeviceCallback();
    private TextView      mAutoTextView;
    private AudioManager  mAudioManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_device_report);
        mAutoTextView = (TextView) findViewById(R.id.text_log);
        mAutoTextView.setMovementMethod(new ScrollingMovementMethod());

        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    }

    @Override
    protected void onStart() {
        super.onStart();
        addAudioDeviceCallback();
    }

    @Override
    protected void onStop() {
        removeAudioDeviceCallback();
        super.onStop();
    }

    @TargetApi(23)
    private void addAudioDeviceCallback(){
        // Note that we will immediately receive a call to onDevicesAdded with the list of
        // devices which are currently connected.
        mAudioManager.registerAudioDeviceCallback(mDeviceCallback, null);
    }

    @TargetApi(23)
    private void removeAudioDeviceCallback(){
        mAudioManager.unregisterAudioDeviceCallback(mDeviceCallback);
    }

    private void reportDeviceInfo(Collection<AudioDeviceInfo> devices) {
        logClear();
        StringBuffer report = new StringBuffer();
        report.append("Device Report:\n");
        for (AudioDeviceInfo deviceInfo : devices) {
            report.append("\n==== Device =================== " + deviceInfo.getId() + "\n");
            String item = AudioDeviceInfoConverter.toString(deviceInfo);
            report.append(item);
        }
        report.append(reportAllMicrophones());
        report.append(reportExtraDeviceInfo());
        log(report.toString());
    }

    public String reportAllMicrophones() {
        StringBuffer report = new StringBuffer();
        report.append("\n############################");
        report.append("\nMicrophone Report:\n");
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P) {
            try {
                List<MicrophoneInfo> micList = mAudioManager.getMicrophones();
                for (MicrophoneInfo micInfo : micList) {
                    String micItem = MicrophoneInfoConverter.reportMicrophoneInfo(micInfo);
                    report.append(micItem);
                }
            } catch (IOException e) {
                e.printStackTrace();
                return e.getMessage();
            }
        } else {
            report.append("\nMicrophoneInfo not available on V" + android.os.Build.VERSION.SDK_INT);
        }
        return report.toString();
    }

    private String reportExtraDeviceInfo() {
        StringBuffer report = new StringBuffer();
        report.append("\n\n############################");
        report.append("\nExtras:");
        String unprocessedSupport = mAudioManager.getParameters(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED);
        report.append("\nSUPPORT_UNPROCESSED  : " + ((unprocessedSupport == null) ?  "null" : "yes"));

        report.append("\nProAudio Feature     : "
            + getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_PRO));
        report.append("\nLowLatency Feature   : "
                + getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY));
        report.append("\nMIDI Feature         : "
                + getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI));
        report.append("\nUSB Host Feature     : "
                + getPackageManager().hasSystemFeature(PackageManager.FEATURE_USB_HOST));
        report.append("\nUSB Accessory Feature: "
                + getPackageManager().hasSystemFeature(PackageManager.FEATURE_USB_ACCESSORY));

        return report.toString();
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

}
