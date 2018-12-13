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

import android.os.Bundle;
import android.view.View;
import android.widget.CheckBox;

import com.google.sample.oboe.manualtest.R;

/**
 * Base class for output test activities
 */
public final class TestOutputActivity extends TestOutputActivityBase {

    public static final int MAX_CHANNEL_BOXES = 8;
    private CheckBox[] mChannelBoxes;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_test_output);

        findAudioCommon();
        updateEnabledWidgets();

        mAudioStreamTester = mAudioOutTester = AudioOutputTester.getInstance();

        mChannelBoxes = new CheckBox[MAX_CHANNEL_BOXES];
        int ic = 0;
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox0);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox1);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox2);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox3);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox4);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox5);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox6);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox7);
        configureChannelBoxes(0);
    }

    public void openAudio() {
        super.openAudio();
        int channelCount = mAudioOutTester.getCurrentAudioStream().getChannelCount();
        configureChannelBoxes(channelCount);
    }

    private void configureChannelBoxes(int channelCount) {
        for (int i = 0; i < mChannelBoxes.length; i++) {
            mChannelBoxes[i].setChecked(i < channelCount);
            mChannelBoxes[i].setEnabled(i < channelCount);
        }
    }

    public void startAudio() {
        super.startAudio();
        mAudioOutTester.setToneType(OboeAudioOutputStream.TONE_TYPE_SINE_STEADY);
        mAudioOutTester.setEnabled(true);
    }

    public void stopAudio() {
        mAudioOutTester.setEnabled(false);
        super.stopAudio();
    }

    public void closeAudio() {
        configureChannelBoxes(0);
        super.closeAudio();
    }

    public void onChannelBoxClicked(View view) {
        CheckBox checkBox = (CheckBox) view;
        String text = (String) checkBox.getText();
        int channelIndex = Integer.parseInt(text);
        mAudioOutTester.setChannelEnabled(channelIndex, checkBox.isChecked());
    }
}
