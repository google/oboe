/*
 * Copyright 2020 The Android Open Source Project
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

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.widget.TextView;

import com.google.sample.audio_device.AudioDeviceInfoConverter;

public class TestDataPathsActivity  extends BaseAutoGlitchActivity {

    public static final int DURATION_SECONDS = 3;
    private double mMagnitude;

    AudioManager mAudioManager;

    // Periodically query for glitches from the native detector.
    protected class DataPathSniffer extends NativeSniffer {

        @Override
        public void startSniffer() {
            long now = System.currentTimeMillis();
            mMagnitude = 0.0;
            super.startSniffer();
        }

        public void run() {
            mMagnitude = getMagnitude();
            reschedule();
        }

        public String getCurrentStatusReport() {
            StringBuffer message = new StringBuffer();
            message.append(getMagnitudeText() + "\n");
            return message.toString();
        }

        @Override
        public String getShortReport() {
            return getMagnitudeText();
        }

        @Override
        public void updateStatusText() {
            mLastGlitchReport = getCurrentStatusReport();
            setAnalyzerText(mLastGlitchReport);
        }

        @Override
        public double getMaxSecondsWithNoGlitch() {
            return -1.0;
        }
    }

    @Override
    NativeSniffer createNativeSniffer() {
        return new TestDataPathsActivity.DataPathSniffer();
    }
    native double getMagnitude();

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_data_paths);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    }


    String getMagnitudeText() {
        return String.format("mag = %7.5f", mMagnitude);
    }

    private static final int[] INPUT_PRESETS = {
            StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
            StreamConfiguration.INPUT_PRESET_GENERIC,
            StreamConfiguration.INPUT_PRESET_CAMCORDER,
            StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
            StreamConfiguration.INPUT_PRESET_UNPROCESSED,
            StreamConfiguration.INPUT_PRESET_VOICE_PERFORMANCE,
    };

    protected String getConfigText(StreamConfiguration config) {
        return (super.getConfigText(config)
                + ", "
                + ((config.getDirection() == StreamConfiguration.DIRECTION_INPUT)
                ? (" inPre = " + StreamConfiguration.convertInputPresetToText(config.getInputPreset()))
                : ""));
    }

    public boolean didTestPass() {
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester. actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester. actualConfiguration;
        boolean passed = true;
        passed &= mMagnitude > 0.001;
        passed &= requestedInConfig.getPerformanceMode() == actualInConfig.getPerformanceMode();
        passed &= requestedOutConfig.getPerformanceMode() == actualOutConfig.getPerformanceMode();
        return passed;
    }

    void testPresetCombo(int inputPreset) throws InterruptedException {
        testPresetCombo(inputPreset, 1, 0, 1, 0);
    }

    void testPresetCombo(int inputPreset,
                         int numInputChannels,
                         int inputChannel,
                         int numOutputChannels,
                         int outputChannel
                   ) throws InterruptedException {
        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();
        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);

        requestedInConfig.setInputPreset(inputPreset);

        requestedInConfig.setChannelCount(numInputChannels);
        requestedOutConfig.setChannelCount(numOutputChannels);

        // FIXME - input not supported
        setOutputChannel(outputChannel);

        mMagnitude = -1.0;
        testConfigurations();
        String summary =
                " ch: in = " + numInputChannels + "[" + inputChannel + "]"
                + ", out = " + numOutputChannels + "[" + outputChannel + "]"
                + ", inPre = "
                + StreamConfiguration.convertInputPresetToText(inputPreset)
                + ", " + getMagnitudeText()
                + "\n";
        appendSummary(summary);
    }

    @Override
    int getActivityType() {
        return ACTIVITY_DATA_PATHS;
    }


    private void testInputPresets() throws InterruptedException {
        for (int inputPreset : INPUT_PRESETS) {
            testPresetCombo(inputPreset);
        }
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
                1, 0, 2, 0);
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
                1, 0, 2, 1);
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION, 2, 0, 2, 0);
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION, 2, 0, 2, 1);
    }

    void testOutputDeviceCombo(int deviceId,
                         int numOutputChannels,
                         int outputChannel
    ) throws InterruptedException {
        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();
        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);

        requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
        requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
        
        requestedInConfig.setChannelCount(1);
        requestedOutConfig.setChannelCount(numOutputChannels);

        setOutputChannel(outputChannel);

        mMagnitude = -1.0;
        testConfigurations();
        String summary =
                " ch: in = " + 1
                        + ", out = " + numOutputChannels + "[" + outputChannel + "]"
                        + ", devId = " + deviceId
                        + ", " + getMagnitudeText()
                        + "\n";
        appendSummary(summary);
    }

    void testOutputDevices() throws InterruptedException {
        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
        for (AudioDeviceInfo deviceInfo : devices) {
            log(AudioDeviceInfoConverter.toString(deviceInfo) + "\n");
            final int SPEAKER_SAFE = 0x18; // API 30
            if (deviceInfo.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
                || deviceInfo.getType() == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE
                || deviceInfo.getType() == SPEAKER_SAFE) {
                int id = deviceInfo.getId();
                int[] channelCounts = deviceInfo.getChannelCounts();
                log("channelCounts.len = " + channelCounts.length + "\n");
                if (channelCounts.length == 0) {
                    testOutputDeviceCombo(id, 1, 0);
                    testOutputDeviceCombo(id, 2, 0);
                    testOutputDeviceCombo(id, 2, 1);
                } else {
                    for (int numChannels : channelCounts) {
                        log("numChannels = " + numChannels + "\n");
                        for (int channel = 0; channel < numChannels; channel++) {
                            testOutputDeviceCombo(id, numChannels, channel);
                        }
                    }
                }
            }
        }
    }

    @Override
    public void runTest() {
        try {
            mDurationSeconds = DURATION_SECONDS;
//            testInputPresets();
            testOutputDevices();

        } catch (Exception e) {
            //log(e.getMessage());
            showErrorToast(e.getMessage());
        }
    }
}
