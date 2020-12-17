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

/**
 * Play a recognizable tone on each channel of each speaker device
 * and listen for the result through a microphone.
 * Also test each microphone channel and device.
 * Try each InputPreset.
 *
 * The analysis is based on a cosine transform of a single
 * frequency. The magnitude indicates the level.
 * The variations in phase, "jitter" indicate how noisy the
 * signal is or whether it is corrupted. A noisy room may have
 * energy at the target frequency but the phase will be random.
 *
 * This test requires a quiet room but no other hardware.
 */
public class TestDataPathsActivity  extends BaseAutoGlitchActivity {

    public static final int DURATION_SECONDS = 3;
    private final static double MIN_REQUIRED_MAGNITUDE = 0.001;
    private final static double MAX_SINE_FREQUENCY = 1000.0;
    private final static int TYPICAL_SAMPLE_RATE = 48000;
    private final static double FRAMES_PER_CYCLE = TYPICAL_SAMPLE_RATE / MAX_SINE_FREQUENCY;
    private final static double PHASE_PER_BIN = 2.0 * Math.PI / FRAMES_PER_CYCLE;
    private final static double MAX_ALLOWED_JITTER = 0.5 * PHASE_PER_BIN;
    // Start by failing then let good results drive us into a pass value.
    private final static double INITIAL_JITTER = 2.0 * MAX_ALLOWED_JITTER;
    // A coefficient of 0.0 is no filtering. 0.9999 is extreme low pass.
    private final static double JITTER_FILTER_COEFFICIENT = 0.8;
    private final static String MAGNITUDE_FORMAT = "%7.5f";

    final int TYPE_BUILTIN_SPEAKER_SAFE = 0x18; // API 30

    private double mMagnitude;
    private double mMaxMagnitude;
    private int    mPhaseCount;
    private double mPhase;
    private double mPhaseJitter;

    AudioManager   mAudioManager;

    private static final int[] INPUT_PRESETS = {
            // VOICE_RECOGNITION gets tested in testInputs()
            // StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
            StreamConfiguration.INPUT_PRESET_GENERIC,
            StreamConfiguration.INPUT_PRESET_CAMCORDER,
            // TODO Resolve issue with echo cancellation killing the signal.
            // TODO StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
            StreamConfiguration.INPUT_PRESET_UNPROCESSED,
            StreamConfiguration.INPUT_PRESET_VOICE_PERFORMANCE,
    };

    // Periodically query for magnitude and phase from the native detector.
    protected class DataPathSniffer extends NativeSniffer {

        @Override
        public void startSniffer() {
            mMagnitude = -1.0;
            mMaxMagnitude = -1.0;
            mPhaseCount = 0;
            mPhase = 0.0;
            mPhaseJitter = INITIAL_JITTER;
            super.startSniffer();
        }

        @Override
        public void run() {
            mMagnitude = getMagnitude();
            mMaxMagnitude = getMaxMagnitude();
            // Only look at the phase if we have a signal.
            if (mMagnitude >= MIN_REQUIRED_MAGNITUDE) {
                double phase = getPhase();
                if (mPhaseCount > 3) {
                    double diff = Math.abs(phase - mPhase);
                    // low pass filter
                    mPhaseJitter = (mPhaseJitter * JITTER_FILTER_COEFFICIENT)
                            + ((diff * (1.0 - JITTER_FILTER_COEFFICIENT)));
                }
                mPhase = phase;
                mPhaseCount++;
            }
            reschedule();
        }

        public String getCurrentStatusReport() {
            StringBuffer message = new StringBuffer();
            message.append(
                    "magnitude = " + getMagnitudeText(mMagnitude)
                    + ", max = " + getMagnitudeText(mMaxMagnitude)
                    + "\nphase = " + getMagnitudeText(mPhase)
                    + ", jitter = " + getMagnitudeText(mPhaseJitter)
                    + "\n");
            return message.toString();
        }

        @Override
        public String getShortReport() {
            return "maxMag = " + getMagnitudeText(mMaxMagnitude)
                    + ", jitter = " + getMagnitudeText(mPhaseJitter);
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
    native double getMaxMagnitude();
    native double getPhase();

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

    protected String getConfigText(StreamConfiguration config) {
        String text = super.getConfigText(config);
        if (config.getDirection() == StreamConfiguration.DIRECTION_INPUT) {
            text += ", inPre = " + StreamConfiguration.convertInputPresetToText(config.getInputPreset());
        }
        return text;
    }

    @Override
    protected String shouldTestBeSkipped() {
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        // No point running the test if we don't get the sharing mode we requested.
        if (actualInConfig.isMMap() != requestedInConfig.isMMap()
                || actualOutConfig.isMMap() != requestedOutConfig.isMMap()) {
            log("Did not get requested MMap stream");
            why += "mmap";
        }        // Did we request a device and not get that device?
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
        if ((requestedInConfig.getInputPreset() != actualInConfig.getInputPreset())) {
            why += ", inPre(" + requestedInConfig.getInputPreset()
                    + "!=" + actualInConfig.getInputPreset() + ")";
        }
        return why;
    }

    @Override
    protected boolean isFinishedEarly() {
        return (mMaxMagnitude > MIN_REQUIRED_MAGNITUDE) && (mPhaseJitter < MAX_ALLOWED_JITTER);
    }

    // @return reasons for failure of empty string
    @Override
    public String didTestFail() {
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        boolean passed = true;
        if (mMaxMagnitude <= MIN_REQUIRED_MAGNITUDE) {
            why += ", mag";
        }
        if (mPhaseJitter > MAX_ALLOWED_JITTER) {
            why += ", jitter";
        }
        return why;
    }

    String getOneLineSummary() {
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        return "#" + mAutomatedTestRunner.getTestCount()
                + ", IN" + (actualInConfig.isMMap() ? "-M" : "-L")
                + " D=" + actualInConfig.getDeviceId()
                + ", ch=" + actualInConfig.getChannelCount() + "[" + getInputChannel() + "]"
                + ", OUT" + (actualOutConfig.isMMap() ? "-M" : "-L")
                + " D=" + (actualOutConfig.isMMap() ? "-M" : "-L")
                + ", ch=" + actualOutConfig.getChannelCount() + "[" + getOutputChannel() + "]"
                + ", mag = " + getMagnitudeText(mMaxMagnitude);
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

        requestedInConfig.setMMap(false);
        requestedOutConfig.setMMap(false);

        setInputChannel(inputChannel);
        setOutputChannel(outputChannel);
    }

    void testPresetCombo(int inputPreset,
                         int numInputChannels,
                         int inputChannel,
                         int numOutputChannels,
                         int outputChannel,
                         boolean mmapEnabled
                   ) throws InterruptedException {

        setupDeviceCombo(numInputChannels, inputChannel, numOutputChannels, outputChannel);

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        requestedInConfig.setInputPreset(inputPreset);
        requestedInConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        int result = testConfigurations();
        if (result != TEST_RESULT_SKIPPED) {
            String summary = getOneLineSummary()
                    + ", inPre = "
                    + StreamConfiguration.convertInputPresetToText(inputPreset)
                    + "\n";
            appendSummary(summary);
            if (result == TEST_RESULT_FAILED) {
                if (inputPreset == StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION) {
                    logFailed("Maybe sine wave blocked by Echo Cancellation!");
                }
            }
        }
    }

    void testPresetCombo(int inputPreset,
                         int numInputChannels,
                         int inputChannel,
                         int numOutputChannels,
                         int outputChannel
    ) throws InterruptedException {
        if (NativeEngine.isMMapSupported()) {
            testPresetCombo(inputPreset, numInputChannels, inputChannel,
                    numOutputChannels, outputChannel, true);
        }
        testPresetCombo(inputPreset, numInputChannels, inputChannel,
                numOutputChannels, outputChannel, false);
    }

    void testPresetCombo(int inputPreset) throws InterruptedException {
        testPresetCombo(inputPreset, 1, 0, 1, 0);
    }

    private void testInputPresets() throws InterruptedException {
        logBoth("\nTest InputPreset -------");

        for (int inputPreset : INPUT_PRESETS) {
            testPresetCombo(inputPreset);
        }
// TODO Resolve issue with echo cancellation killing the signal.
//        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
//                1, 0, 2, 0);
//        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
//                1, 0, 2, 1);
//        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
//                2, 0, 2, 0);
//        testPresetCombo(StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION,
//                2, 0, 2, 1);
    }

    void testInputDeviceCombo(int deviceId,
                              int numInputChannels,
                              int inputChannel,
                              boolean mmapEnabled) throws InterruptedException {
        final int numOutputChannels = 2;
        setupDeviceCombo(numInputChannels, inputChannel, numOutputChannels, 0);

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        requestedInConfig.setInputPreset(StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION);
        requestedInConfig.setDeviceId(deviceId);
        requestedInConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        int result = testConfigurations();
        if (result != TEST_RESULT_SKIPPED) {
            appendSummary(getOneLineSummary() + "\n");
        }
    }

    void testInputDeviceCombo(int deviceId,
                              int numInputChannels,
                              int inputChannel) throws InterruptedException {
        if (NativeEngine.isMMapSupported()) {
            testInputDeviceCombo(deviceId, numInputChannels, inputChannel, true);
        }
        testInputDeviceCombo(deviceId, numInputChannels, inputChannel, false);
    }

    void testInputDevices() throws InterruptedException {
        logBoth("\nTest Input Devices -------");

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
                               int deviceType,
                               int numOutputChannels,
                               int outputChannel,
                               boolean mmapEnabled) throws InterruptedException {
        final int numInputChannels = 2; // TODO review, done because of mono problems on some devices
        setupDeviceCombo(numInputChannels, 0, numOutputChannels, outputChannel);

        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        requestedOutConfig.setDeviceId(deviceId);
        requestedOutConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        int result = testConfigurations();
        if (result != TEST_RESULT_SKIPPED) {
            appendSummary(getOneLineSummary() + "\n");
            if (result == TEST_RESULT_FAILED) {
                if (deviceType == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE
                        && numOutputChannels == 2
                        && outputChannel == 1) {
                    logFailed("Maybe EARPIECE does not mix stereo to mono!");
                }
                if (deviceType == TYPE_BUILTIN_SPEAKER_SAFE
                        && numOutputChannels == 2
                        && outputChannel == 0) {
                    logFailed("Maybe SPEAKER_SAFE blocked channel 0!");
                }
            }
        }
    }

    void testOutputDeviceCombo(int deviceId,
                               int deviceType,
                               int numOutputChannels,
                               int outputChannel) throws InterruptedException {
        if (NativeEngine.isMMapSupported()) {
            testOutputDeviceCombo(deviceId, deviceType, numOutputChannels, outputChannel, true);
        }
        testOutputDeviceCombo(deviceId, deviceType, numOutputChannels, outputChannel, false);
    }

    void logBoth(String text) {
        log(text);
        appendSummary(text + "\n");
    }
    void logFailed(String text) {
        log(text);
        appendFailedSummary(text + "\n");
    }

    void testOutputDevices() throws InterruptedException {
        logBoth("\nTest Output Devices -------");

        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
        int numTested = 0;
        for (AudioDeviceInfo deviceInfo : devices) {
            log("----\n"
                    + AudioDeviceInfoConverter.toString(deviceInfo) + "\n");
            if (!deviceInfo.isSink()) continue;
            int deviceType = deviceInfo.getType();
            if (deviceType == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
                || deviceType == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE
                || deviceType == TYPE_BUILTIN_SPEAKER_SAFE) {
                int id = deviceInfo.getId();
                int[] channelCounts = deviceInfo.getChannelCounts();
                numTested++;
                // Always test mono and stereo.
                testOutputDeviceCombo(id, deviceType, 1, 0);
                testOutputDeviceCombo(id, deviceType, 2, 0);
                testOutputDeviceCombo(id, deviceType, 2, 1);
                if (channelCounts.length > 0) {
                    for (int numChannels : channelCounts) {
                        // Test higher channel counts.
                        if (numChannels > 2) {
                            log("numChannels = " + numChannels + "\n");
                            for (int channel = 0; channel < numChannels; channel++) {
                                testOutputDeviceCombo(id, deviceType, numChannels, channel);
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

            log("min.required.magnitude = " + MIN_REQUIRED_MAGNITUDE);
            log("max.allowed.jitter = " + MAX_ALLOWED_JITTER);
            log("test.gap.msec = " + mGapMillis);

            testInputPresets();
            testInputDevices();
            testOutputDevices();
        } catch (Exception e) {
            log(e.getMessage());
            showErrorToast(e.getMessage());
        }
    }

}
