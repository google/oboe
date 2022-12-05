/*
 * Copyright 2022 The Android Open Source Project
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

package com.mobileer.oboetester;

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.mobileer.audio_device.CommunicationDeviceSpinner;

public class CommunicationDeviceView extends LinearLayout {
    private CheckBox mSpeakerphoneCheckbox;
    private TextView mIsSpeakerphoneText;
    private AudioManager mAudioManager;
    private CommunicationDeviceSpinner mDeviceSpinner;

    public CommunicationDeviceView(Context context) {
        super(context);
        initializeViews(context);
    }

    public CommunicationDeviceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public CommunicationDeviceView(Context context,
                          AttributeSet attrs,
                          int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    /**
     * Inflates the views in the layout.
     *
     * @param context the current context for the view.
     */
    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.comm_device_view, this);

        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        mSpeakerphoneCheckbox = (CheckBox) findViewById(R.id.setSpeakerphoneOn);

        mDeviceSpinner = (CommunicationDeviceSpinner) findViewById(R.id.comm_devices_spinner);
        mDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id)
            {
                AudioDeviceInfo[] commDeviceArray = mDeviceSpinner.getCommunicationsDevices();
                if (commDeviceArray != null) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        if (position == 0) {
                            mAudioManager.clearCommunicationDevice();
                        } else {
                            AudioDeviceInfo selectedDevice = commDeviceArray[position - 1]; // skip "Clear"
                            mAudioManager.setCommunicationDevice(selectedDevice);
                        }
                        showCommDeviceStatus();
                    }
                }
            }
            public void onNothingSelected(AdapterView<?> parent) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    mAudioManager.clearCommunicationDevice();
                }
                showCommDeviceStatus();
            }
        });

        mSpeakerphoneCheckbox.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onSetSpeakerphoneOn(view);
            }
        });
        mIsSpeakerphoneText = (TextView) findViewById(R.id.isSpeakerphoneOn);
        showCommDeviceStatus();
    }

    public void cleanup() {
        mSpeakerphoneCheckbox.setChecked(false);
        setSpeakerPhoneOn(false);
    }

    public void onSetSpeakerphoneOn(View view) {
        Log.d(TestAudioActivity.TAG, "onSetSpeakerphoneOn() called from Checkbox");
        CheckBox checkBox = (CheckBox) view;
        boolean enabled = checkBox.isChecked();
        setSpeakerPhoneOn(enabled);
        showCommDeviceStatus();
    }

    private void setSpeakerPhoneOn(boolean enabled) {
        Log.d(TestAudioActivity.TAG, "call setSpeakerphoneOn(" + enabled + ")");
        mAudioManager.setSpeakerphoneOn(enabled);
    }

    private void showCommDeviceStatus() {
        boolean enabled = mAudioManager.isSpeakerphoneOn();
        String text = (enabled ? "ON" : "OFF");
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            AudioDeviceInfo commDeviceInfo = mAudioManager.getCommunicationDevice();
            if (commDeviceInfo != null) {
                text += ", CommDev=" + commDeviceInfo.getId();
            }
        }
        mIsSpeakerphoneText.setText(" => " + text);
    }

}
