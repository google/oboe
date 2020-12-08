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

import com.google.sample.audio_device.AudioDeviceInfoConverter;

public class TestDataPathsActivity  extends BaseAutoGlitchActivity {

    public static final int DURATION_SECONDS = 3;
    private double mMagnitude;
    private double mMaxMagnitude;
    private final static String MAGNITUDE_FORMAT = "%7.5f";

    AudioManager mAudioManager;

    // Periodically query for glitches from the native detector.
    protected class DataPathSniffer extends NativeSniffer {

        @Override
        public void startSniffer() {
            mMagnitude = 0.0;
            mMaxMagnitude = 0.0;
            super.startSniffer();
        }

        @Override
        public void run() {
            mMagnitude = getMagnitude();
            mMaxMagnitude = Math.max(mMagnitude, mMaxMagnitude);
            reschedule();
        }

        public String getCurrentStatusReport() {
            StringBuffer message = new StringBuffer();
            message.append(
                    "magnitude = " + getMagnitudeText(mMagnitude)
                    + ", max = " + getMagnitudeText(mMaxMagnitude)
                    + "\n");
            return message.toString();
        }

        @Override
        public String getShortReport() {
            return "maxMag = " + getMagnitudeText(mMaxMagnitude);
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

    @Override
    public String getTestName() {
        return "DataPaths";
    }

    @Override
    int getActivityType() {
        return ACTIVITY_DATA_PATHS;
    }

    String getMagnitudeText(double value) {
        return String.format(MAGNITUDE_FORMAT, value);
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
        String text = super.getConfigText(config);
        if (config.getDirection() == StreamConfiguration.DIRECTION_INPUT) {
            text += ", inPre = " + StreamConfiguration.convertInputPresetToText(config.getInputPreset());
        }
        return text;
    }

    // @return reasons for failure of empty string
    @Override
    public String didTestPass() {
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        boolean passed = true;
        if (mMaxMagnitude <= 0.001) {
            why += ", mag";
        }
        if (requestedInConfig.getPerformanceMode() != actualInConfig.getPerformanceMode()) {
            why += ", inPerf";
        }
        if (requestedOutConfig.getPerformanceMode() != actualOutConfig.getPerformanceMode()) {
            why += ", outPerf";
        }
        // Did we request a device and not get that device?
        if (requestedInConfig.getDeviceId() != 0
                && (requestedInConfig.getDeviceId() != actualInConfig.getDeviceId())) {
            why += ", inDev(" + requestedInConfig.getDeviceId()
                    + "!=" + actualInConfig.getDeviceId() + ")";
        }
        if (requestedOutConfig.getDeviceId() != 0
                && (requestedOutConfig.getDeviceId() != actualOutConfig.getDeviceId())) {
            why += ", outDev(" + requestedOutConfig.getDeviceId()
                    + "!=" + actualOutConfig.getDeviceId() + ")";
        }
        return why;
    }

    String getOneLineSummary() {
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        return "#" + mAutomatedTestRunner.getTestCount()
                + ", IN D=" + actualInConfig.getDeviceId()
                + ", ch=" + actualInConfig.getChannelCount() + "[" + getInputChannel() + "]"
                + ", OUT D=" + actualOutConfig.getDeviceId()
                + ", ch=" + actualOutConfig.getChannelCount() + "[" + getOutputChannel() + "]"
                + ", mag = " + getMagnitudeText(mMaxMagnitude);
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

        setInputChannel(inputChannel);
        setOutputChannel(outputChannel);

        mMagnitude = -1.0;
        testConfigurations();
        String summary = getOneLineSummary()
                + ", inPre = "
                + StreamConfiguration.convertInputPresetToText(inputPreset)
                + "\n";
        appendSummary(summary);
    }

    void testPresetCombo(int inputPreset) throws InterruptedException {
        testPresetCombo(inputPreset, 1, 0, 1, 0);
    }

    private void testInputPresets() throws InterruptedException {
        logBoth("\nTest InputPreset -------\n");

        for (int inputPreset : INPUT_PRESETS) {
            testPresetCombo(inputPreset);
        }
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
                1, 0, 2, 0);
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
                1, 0, 2, 1);
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
                2, 0, 2, 0);
        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
                2, 0, 2, 1);
    }

    void setupDeviceCombo(int numInputChannels,
                         int inputChannel,
                         int numOutputChannels,
                         int outputChannel) throws InterruptedException {
        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();
        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);

        requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
        requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);

        requestedInConfig.setChannelCount(numInputChannels);
        requestedOutConfig.setChannelCount(numOutputChannels);

        setInputChannel(inputChannel);
        setOutputChannel(outputChannel);
    }

    void testInputDeviceCombo(int deviceId,
                              int numInputChannels,
                              int inputChannel) throws InterruptedException {
            final int numOutputChannels = 2;
            setupDeviceCombo(numInputChannels, inputChannel, numOutputChannels, 0);
            StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
            requestedInConfig.setDeviceId(deviceId);

            mMagnitude = -1.0;
            testConfigurations();
            appendSummary(getOneLineSummary() + "\n");
        }

    void testInputDevices() throws InterruptedException {
        logBoth("\nTest Input Devices -------\n");

        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_INPUTS);
        int numTested = 0;
        for (AudioDeviceInfo deviceInfo : devices) {
            log("----\n"
                    + AudioDeviceInfoConverter.toString(deviceInfo) + "\n");
            if (!deviceInfo.isSource()) continue; // FIXME log as error?!
            if (deviceInfo.getType() == AudioDeviceInfo.TYPE_BUILTIN_MIC) {
                int id = deviceInfo.getId();
                int[] channelCounts = deviceInfo.getChannelCounts();
                numTested++;
                // Always test mono and stereo.
                testInputDeviceCombo(id, 1, 0);
                testInputDeviceCombo(id, 2, 0);
                testInputDeviceCombo(id, 2, 1);
                if (channelCounts.length > 0) {
                    for (int numChannels : channelCounts) {
                        // Test higher channel counts.
                        if (numChannels > 2) {
                            log("numChannels = " + numChannels + "\n");
                            for (int channel = 0; channel < numChannels; channel++) {
                                testInputDeviceCombo(id, numChannels, channel);
                            }
                        }
                    }
                }
            } else {
                log("Device skipped for type.");
            }
        }

        if (numTested == 0) {
            log("NO INPUT DEVICE FOUND!\n");
        }
    }

    void testOutputDeviceCombo(int deviceId,
                               int numOutputChannels,
                               int outputChannel) throws InterruptedException {
        final int numInputChannels = 2;
        setupDeviceCombo(numInputChannels, 0, numOutputChannels, outputChannel);
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        requestedOutConfig.setDeviceId(deviceId);

        mMagnitude = -1.0;
        testConfigurations();
        appendSummary(getOneLineSummary() + "\n");
    }

    void logBoth(String text) {
        log(text);
        appendSummary(text);
    }

    void testOutputDevices() throws InterruptedException {
        logBoth("\nTest Output Devices -------\n");

        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
        int numTested = 0;
        for (AudioDeviceInfo deviceInfo : devices) {
            log("----\n"
                    + AudioDeviceInfoConverter.toString(deviceInfo) + "\n");
            final int SPEAKER_SAFE = 0x18; // API 30
            if (!deviceInfo.isSink()) continue;
            if (deviceInfo.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
                || deviceInfo.getType() == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE
                || deviceInfo.getType() == SPEAKER_SAFE) {
                int id = deviceInfo.getId();
                int[] channelCounts = deviceInfo.getChannelCounts();
                numTested++;
                // Always test mono and stereo.
                testOutputDeviceCombo(id, 1, 0);
                testOutputDeviceCombo(id, 2, 0);
                testOutputDeviceCombo(id, 2, 1);
                if (channelCounts.length > 0) {
                    for (int numChannels : channelCounts) {
                        // Test higher channel counts.
                        if (numChannels > 2) {
                            log("numChannels = " + numChannels + "\n");
                            for (int channel = 0; channel < numChannels; channel++) {
                                testOutputDeviceCombo(id, numChannels, channel);
                            }
                        }
                    }
                }
            } else {
                log("Device skipped for type.");
            }
        }
        if (numTested == 0) {
            log("NO OUTPUT DEVICE FOUND!\n");
        }
    }

    @Override
    public void runTest() {
        try {
            mDurationSeconds = DURATION_SECONDS;
            testInputPresets();
            testInputDevices();
            testOutputDevices();
        } catch (Exception e) {
            //log(e.getMessage());
            showErrorToast(e.getMessage());
        }
    }
}
