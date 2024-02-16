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

package com.mobileer.oboetester;

import static com.mobileer.oboetester.IntentBasedTestSupport.configureStreamsFromBundle;
import static com.mobileer.oboetester.StreamConfiguration.convertChannelMaskToText;

import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.mobileer.audio_device.AudioDeviceInfoConverter;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Locale;

/**
 * TODO
 * Select device types based on priority or maybe offer checkboxes.
 * Print devices types being tested.
 * Avoid mismatch such as mic input and USB output.
 * Avoid duplicating tests by not using Unspecified (except for ch-mask)
 *
 * Find available device types.
 * Decide which device types to test.
 * For each type find partner type, then decide other options:
 * channels for speaker/mic and wired headset usb headset get mixed to one
 *     so allow channel mask downmix test
 *     check each channelCount if specified
 * channels for USB device are 1:1, so match channels directly, FFT per input?
 *
 */

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
 * This test requires a quiet room if you are testing speaker/mic pairs.
 * It can also be used to test using analog loopback adapters
 * or USB devices configured in loopback mode.
 *
 *
 */
public class TestDataPathsActivity  extends BaseAutoGlitchActivity {

    public static final String KEY_USE_INPUT_PRESETS = "use_input_presets";
    public static final boolean VALUE_DEFAULT_USE_INPUT_PRESETS = true;

    public static final String KEY_USE_INPUT_DEVICES = "use_input_devices";
    public static final boolean VALUE_DEFAULT_USE_INPUT_DEVICES = true;

    public static final String KEY_USE_OUTPUT_DEVICES = "use_output_devices";
    public static final boolean VALUE_DEFAULT_USE_OUTPUT_DEVICES = true;

    public static final String KEY_USE_ALL_OUTPUT_CHANNEL_MASKS = "use_all_output_channel_masks";
    public static final boolean VALUE_DEFAULT_USE_ALL_OUTPUT_CHANNEL_MASKS = false;

    public static final String KEY_USE_ALL_SAMPLE_RATES = "use_all_sample_rates";
    public static final boolean VALUE_DEFAULT_USE_ALL_SAMPLE_RATES = false;

    public static final String KEY_SINGLE_TEST_INDEX = "single_test_index";
    public static final int VALUE_DEFAULT_SINGLE_TEST_INDEX = -1;

    public static final int DURATION_SECONDS = 3;
    private final static double MIN_REQUIRED_MAGNITUDE = 0.001;
    private final static double MAX_SINE_FREQUENCY = 1000.0;
    private final static int TYPICAL_SAMPLE_RATE = 48000;
    private final static double FRAMES_PER_CYCLE = TYPICAL_SAMPLE_RATE / MAX_SINE_FREQUENCY;
    private final static double PHASE_PER_BIN = 2.0 * Math.PI / FRAMES_PER_CYCLE;
    private final static double MAX_ALLOWED_JITTER = 2.0 * PHASE_PER_BIN;
    private final static String MAGNITUDE_FORMAT = "%7.5f";

    // These define the values returned by the Java API deviceInfo.getChannelMasks().
    public static final int JAVA_CHANNEL_IN_LEFT = 1 << 2;  // AudioFormat.CHANNEL_IN_LEFT
    public static final int JAVA_CHANNEL_IN_RIGHT = 1 << 3; // AudioFormat.CHANNEL_IN_RIGHT
    public static final int JAVA_CHANNEL_IN_FRONT = 1 << 4; // AudioFormat.CHANNEL_IN_FRONT
    public static final int JAVA_CHANNEL_IN_BACK = 1 << 5;  // AudioFormat.CHANNEL_IN_BACK

    // These do not have corresponding Java definitions.
    // They match definitions in system/media/audio/include/system/audio-hal-enums.h
    public static final int JAVA_CHANNEL_IN_BACK_LEFT = 1 << 16;
    public static final int JAVA_CHANNEL_IN_BACK_RIGHT = 1 << 17;
    public static final int JAVA_CHANNEL_IN_CENTER = 1 << 18;
    public static final int JAVA_CHANNEL_IN_LOW_FREQUENCY = 1 << 20;
    public static final int JAVA_CHANNEL_IN_TOP_LEFT = 1 << 21;
    public static final int JAVA_CHANNEL_IN_TOP_RIGHT = 1 << 22;

    public static final int JAVA_CHANNEL_IN_MONO = JAVA_CHANNEL_IN_FRONT;
    public static final int JAVA_CHANNEL_IN_STEREO = JAVA_CHANNEL_IN_LEFT | JAVA_CHANNEL_IN_RIGHT;
    public static final int JAVA_CHANNEL_IN_FRONT_BACK = JAVA_CHANNEL_IN_FRONT | JAVA_CHANNEL_IN_BACK;
    public static final int JAVA_CHANNEL_IN_2POINT0POINT2 = JAVA_CHANNEL_IN_LEFT |
            JAVA_CHANNEL_IN_RIGHT |
            JAVA_CHANNEL_IN_TOP_LEFT |
            JAVA_CHANNEL_IN_TOP_RIGHT;
    public static final int JAVA_CHANNEL_IN_2POINT1POINT2 =
            JAVA_CHANNEL_IN_2POINT0POINT2 | JAVA_CHANNEL_IN_LOW_FREQUENCY;
    public static final int JAVA_CHANNEL_IN_3POINT0POINT2 =
            JAVA_CHANNEL_IN_2POINT0POINT2 | JAVA_CHANNEL_IN_CENTER;
    public static final int JAVA_CHANNEL_IN_3POINT1POINT2 =
            JAVA_CHANNEL_IN_3POINT0POINT2 | JAVA_CHANNEL_IN_LOW_FREQUENCY;
    public static final int JAVA_CHANNEL_IN_5POINT1 = JAVA_CHANNEL_IN_LEFT |
            JAVA_CHANNEL_IN_CENTER |
            JAVA_CHANNEL_IN_RIGHT |
            JAVA_CHANNEL_IN_BACK_LEFT |
            JAVA_CHANNEL_IN_BACK_RIGHT |
            JAVA_CHANNEL_IN_LOW_FREQUENCY;
    public static final int JAVA_CHANNEL_UNDEFINED = -1;

    final int TYPE_BUILTIN_SPEAKER_SAFE = 0x18; // API 30

    private double mMagnitude;
    private double mMaxMagnitude;
    private int    mPhaseCount;
    private double mPhase;
    private double mPhaseErrorSum;
    private double mPhaseErrorCount;

    private CheckBox mCheckBoxInputPresets;
    private CheckBox mCheckBoxInputDevices;
    private CheckBox mCheckBoxOutputDevices;
    private CheckBox mCheckBoxAllOutputChannelMasks;
    private CheckBox mCheckBoxAllSampleRates;
    private TextView mInstructionsTextView;

    private static final int[] INPUT_PRESETS = {
            StreamConfiguration.INPUT_PRESET_GENERIC,
            StreamConfiguration.INPUT_PRESET_CAMCORDER,
            StreamConfiguration.INPUT_PRESET_UNPROCESSED,
            // Do not use INPUT_PRESET_VOICE_COMMUNICATION because AEC kills the signal.
            StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION,
            StreamConfiguration.INPUT_PRESET_VOICE_PERFORMANCE,
    };

    private static final int[] SHORT_OUTPUT_CHANNEL_MASKS = {
            StreamConfiguration.CHANNEL_MONO,
            StreamConfiguration.CHANNEL_STEREO,
            StreamConfiguration.CHANNEL_2POINT1, // TODO should this be 5.1?!
            StreamConfiguration.CHANNEL_7POINT1POINT4,
    };

    private static final int[] ALL_OUTPUT_CHANNEL_MASKS = {
            StreamConfiguration.CHANNEL_MONO,
            StreamConfiguration.CHANNEL_STEREO,
            StreamConfiguration.CHANNEL_2POINT1,
            StreamConfiguration.CHANNEL_TRI,
            StreamConfiguration.CHANNEL_TRI_BACK,
            StreamConfiguration.CHANNEL_3POINT1,
            StreamConfiguration.CHANNEL_2POINT0POINT2,
            StreamConfiguration.CHANNEL_2POINT1POINT2,
            StreamConfiguration.CHANNEL_3POINT0POINT2,
            StreamConfiguration.CHANNEL_3POINT1POINT2,
            StreamConfiguration.CHANNEL_QUAD,
            StreamConfiguration.CHANNEL_QUAD_SIDE,
            StreamConfiguration.CHANNEL_SURROUND,
            StreamConfiguration.CHANNEL_PENTA,
            StreamConfiguration.CHANNEL_5POINT1,
            StreamConfiguration.CHANNEL_5POINT1_SIDE,
            StreamConfiguration.CHANNEL_6POINT1,
            StreamConfiguration.CHANNEL_7POINT1,
            StreamConfiguration.CHANNEL_5POINT1POINT2,
            StreamConfiguration.CHANNEL_5POINT1POINT4,
            StreamConfiguration.CHANNEL_7POINT1POINT2,
            StreamConfiguration.CHANNEL_7POINT1POINT4,
    };

    private static final int[] SAMPLE_RATES = {
            8000,
            11025,
            12000,
            16000,
            22050,
            24000,
            32000,
            44100,
            48000,
            64000,
            88200,
            96000,
    };

    @NonNull
    public static String comparePassedField(String prefix, Object failed, Object passed, String name) {
        try {
            Field field = failed.getClass().getField(name);
            int failedValue = field.getInt(failed);
            int passedValue = field.getInt(passed);
            return (failedValue == passedValue) ? ""
                :  (prefix + " " + name + ": passed = " + passedValue + ", failed = " + failedValue + "\n");
        } catch (NoSuchFieldException e) {
            return "ERROR - no such field  " + name;
        } catch (IllegalAccessException e) {
            return "ERROR - cannot access  " + name;
        }
    }

    public static double calculatePhaseError(double p1, double p2) {
        double diff = p1 - p2;
        // Wrap around the circle.
        while (diff > Math.PI) {
            diff -= 2 * Math.PI;
        }
        while (diff < -Math.PI) {
            diff += 2 * Math.PI;
        }
        return diff;
    }

    // Periodically query for magnitude and phase from the native detector.
    protected class DataPathSniffer extends NativeSniffer {

        @Override
        public void startSniffer() {
            mMagnitude = -1.0;
            mMaxMagnitude = -1.0;
            mPhaseCount = 0;
            mPhase = 0.0;
            mPhaseErrorSum = 0.0;
            mPhaseErrorCount = 0;
            super.startSniffer();
        }

        private void gatherData() {
            mMagnitude = getMagnitude();
            mMaxMagnitude = getMaxMagnitude();
            Log.d(TAG, String.format(Locale.getDefault(), "magnitude = %7.4f, maxMagnitude = %7.4f",
                    mMagnitude, mMaxMagnitude));
            // Only look at the phase if we have a signal.
            if (mMagnitude >= MIN_REQUIRED_MAGNITUDE) {
                double phase = getPhaseDataPaths();
                // Wait for the analyzer to get a lock on the signal.
                // Arbitrary number of phase measurements before we start measuring jitter.
                final int kMinPhaseMeasurementsRequired = 4;
                if (mPhaseCount >= kMinPhaseMeasurementsRequired) {
                    double phaseError = Math.abs(calculatePhaseError(phase, mPhase));
                    // collect average error
                    mPhaseErrorSum += phaseError;
                    mPhaseErrorCount++;
                    Log.d(TAG, String.format(Locale.getDefault(), "phase = %7.4f, mPhase = %7.4f, phaseError = %7.4f, jitter = %7.4f",
                            phase, mPhase, phaseError, getAveragePhaseError()));
                }
                mPhase = phase;
                mPhaseCount++;
            }
        }

        public String getCurrentStatusReport() {
            StringBuffer message = new StringBuffer();
            message.append(
                    "magnitude = " + getMagnitudeText(mMagnitude)
                    + ", max = " + getMagnitudeText(mMaxMagnitude)
                    + "\nphase = " + getMagnitudeText(mPhase)
                    + ", jitter = " + getJitterText()
                    + ", #" + mPhaseCount
                    + "\n");
            return message.toString();
        }

        public String getShortReport() {
            return "maxMag = " + getMagnitudeText(mMaxMagnitude)
                    + ", jitter = " + getJitterText();
        }

        @Override
        public void updateStatusText() {
            gatherData();
            mLastGlitchReport = getCurrentStatusReport();
            runOnUiThread(() -> {
                setAnalyzerText(mLastGlitchReport);
            });
        }
    }

    // Write to status and command view
    private void setInstructionsText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mInstructionsTextView.setText(text);
            }
        });
    }

    private String getJitterText() {
        return isPhaseJitterValid() ? getMagnitudeText(getAveragePhaseError()) : "?";
    }

    @Override
    NativeSniffer createNativeSniffer() {
        return new TestDataPathsActivity.DataPathSniffer();
    }

    @Override
    public String getShortReport() {
        return ((DataPathSniffer) mNativeSniffer).getShortReport();
    }

    native double getMagnitude();
    native double getMaxMagnitude();

    native double getPhaseDataPaths();

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_data_paths);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mCheckBoxInputPresets = (CheckBox)findViewById(R.id.checkbox_paths_input_presets);
        mCheckBoxInputDevices = (CheckBox)findViewById(R.id.checkbox_paths_input_devices);
        mCheckBoxOutputDevices = (CheckBox)findViewById(R.id.checkbox_paths_output_devices);
        mCheckBoxAllOutputChannelMasks =
                (CheckBox)findViewById(R.id.checkbox_paths_all_output_channel_masks);
        mCheckBoxAllSampleRates =
                (CheckBox)findViewById(R.id.checkbox_paths_all_sample_rates);

        mInstructionsTextView = (TextView) findViewById(R.id.text_instructions);
    }

    @Override
    public String getTestName() {
        return "DataPaths";
    }

    @Override
    int getActivityType() {
        return ACTIVITY_DATA_PATHS;
    }

    static String getMagnitudeText(double value) {
        return String.format(Locale.getDefault(), MAGNITUDE_FORMAT, value);
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
        String why = super.shouldTestBeSkipped();
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;

        // Did we request a device and not get that device?
        if (requestedInConfig.getDeviceId() != 0
                && (requestedInConfig.getDeviceId() != actualInConfig.getDeviceId())) {
            why += "inDev(" + requestedInConfig.getDeviceId()
                    + "!=" + actualInConfig.getDeviceId() + "),";
        }
        if (requestedOutConfig.getDeviceId() != 0
                && (requestedOutConfig.getDeviceId() != actualOutConfig.getDeviceId())) {
            why += ", outDev(" + requestedOutConfig.getDeviceId()
                    + "!=" + actualOutConfig.getDeviceId() + "),";
        }
        if ((requestedInConfig.getInputPreset() != actualInConfig.getInputPreset())) {
            why += ", inPre(" + requestedInConfig.getInputPreset()
                    + "!=" + actualInConfig.getInputPreset() + "),";
        }
        return why;
    }

    @Override
    protected boolean isFinishedEarly() {
        return (mMaxMagnitude > MIN_REQUIRED_MAGNITUDE)
                && (getAveragePhaseError() < MAX_ALLOWED_JITTER)
                && isPhaseJitterValid();
    }

    // @return reasons for failure of empty string
    @Override
    public String didTestFail() {
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        if (mMaxMagnitude <= MIN_REQUIRED_MAGNITUDE) {
            why += ", mag";
        }
        if (!isPhaseJitterValid()) {
            why += ", jitterUnknown";
        } else if (getAveragePhaseError() > MAX_ALLOWED_JITTER) {
            why += ", jitterHigh";
        }
        return why;
    }

    private double getAveragePhaseError() {
        // If we have no measurements then return maximum possible phase jitter
        // to avoid dividing by zero.
        return (mPhaseErrorCount > 0) ? (mPhaseErrorSum / mPhaseErrorCount) : Math.PI;
    }

    private boolean isPhaseJitterValid() {
        // Arbitrary number of measurements to be considered valid.
        final int kMinPhaseErrorCount = 5;
        return mPhaseErrorCount >= kMinPhaseErrorCount;
    }

    String getOneLineSummary() {
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        return "#" + mAutomatedTestRunner.getTestCount()
                + ", IN" + (actualInConfig.isMMap() ? "-M" : "-L")
                + " D=" + actualInConfig.getDeviceId()
                + ", ch=" + channelText(getInputChannel(), actualInConfig.getChannelCount())
                + ", SR=" + actualInConfig.getSampleRate()
                + ", OUT" + (actualOutConfig.isMMap() ? "-M" : "-L")
                + " D=" + actualOutConfig.getDeviceId()
                + ", ch=" + channelText(getOutputChannel(), actualOutConfig.getChannelCount())
                + ", SR=" + actualOutConfig.getSampleRate()
                + ", mag = " + getMagnitudeText(mMaxMagnitude);
    }

    void setupDeviceCombo(int inputChannelCount,
                          int inputChannelMask,
                          int inputChannel,
                          int inputSampleRate,
                          int outputChannelCount,
                          int outputChannelMask,
                          int outputChannel,
                          int outputSampleRate) throws InterruptedException {
        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();

        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);

        requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
        requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);

        requestedInConfig.setSampleRate(inputSampleRate);
        requestedOutConfig.setSampleRate(outputSampleRate);

        if (inputChannelMask != 0) {
            requestedInConfig.setChannelMask(inputChannelMask);
        } else {
            requestedInConfig.setChannelCount(inputChannelCount);
        }
        if (outputChannelMask != 0) {
            requestedOutConfig.setChannelMask(outputChannelMask);
        } else {
            requestedOutConfig.setChannelCount(outputChannelCount);
        }

        requestedInConfig.setMMap(false);
        requestedOutConfig.setMMap(false);

        setInputChannel(inputChannel);
        setOutputChannel(outputChannel);
    }

    @Override
    protected TestResult testCurrentConfigurations() throws InterruptedException {
        TestResult testResult = super.testCurrentConfigurations();
        if (testResult != null) {
            testResult.addComment("mag = " + TestDataPathsActivity.getMagnitudeText(mMagnitude)
                    + ", jitter = " + getJitterText());

            logOneLineSummary(testResult);
            int result = testResult.result;
            if (result == TEST_RESULT_FAILED) {
                int id = mAudioOutTester.actualConfiguration.getDeviceId();
                int deviceType = getDeviceInfoById(id).getType();
                int channelCount = mAudioOutTester.actualConfiguration.getChannelCount();
                if (deviceType == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE
                        && channelCount == 2
                        && getOutputChannel() == 1) {
                    testResult.addComment("Maybe EARPIECE does not mix stereo to mono!");
                }
                if (deviceType == TYPE_BUILTIN_SPEAKER_SAFE
                        && channelCount == 2
                        && getOutputChannel() == 0) {
                    testResult.addComment("Maybe SPEAKER_SAFE dropped channel zero!");
                }
            }
        }
        return testResult;
    }

    void testPresetCombo(int inputPreset,
                         int numInputChannels,
                         int inputChannel,
                         int numOutputChannels,
                         int outputChannel,
                         boolean mmapEnabled
                   ) throws InterruptedException {
        setupDeviceCombo(numInputChannels, 0, inputChannel, 48000,
                numOutputChannels, 0, outputChannel, 48000);

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        requestedInConfig.setInputPreset(inputPreset);
        requestedInConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        TestResult testResult = testCurrentConfigurations();
        if (testResult != null) {
            int result = testResult.result;
            String extra = ", inPre = "
                    + StreamConfiguration.convertInputPresetToText(inputPreset);
            logOneLineSummary(testResult, extra);
            if (result == TEST_RESULT_FAILED) {
                if (getMagnitude() < 0.000001) {
                    testResult.addComment("The input is completely SILENT!");
                } else if (inputPreset == StreamConfiguration.INPUT_PRESET_VOICE_COMMUNICATION) {
                    testResult.addComment("Maybe sine wave blocked by Echo Cancellation!");
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
        setTestName("Test InPreset = " + StreamConfiguration.convertInputPresetToText(inputPreset));
        testPresetCombo(inputPreset, 1, 0, 1, 0);
    }

    private void testInputPresets() throws InterruptedException {
        logBoth("\nTest InputPreset -------");

        for (int inputPreset : INPUT_PRESETS) {
            testPresetCombo(inputPreset);
        }
    }

    void testInputDeviceCombo(int deviceId,
                              int deviceType,
                              int channelCount,
                              int channelMask,
                              int inputChannel,
                              int inputSampleRate,
                              boolean mmapEnabled) throws InterruptedException {
        String typeString = AudioDeviceInfoConverter.typeToString(deviceType);
        if (channelMask != 0) {
            setTestName("Test InDev: #" + deviceId + " " + typeString + "_" +
                    convertChannelMaskToText(channelMask) + "_" +
                    inputChannel + "/" + channelCount + "_" + inputSampleRate);
        } else {
            setTestName("Test InDev: #" + deviceId + " " + typeString
                    + "_" + inputChannel + "/" + channelCount + "_" + inputSampleRate);
        }

        final int numOutputChannels = 2;
        setupDeviceCombo(channelCount, channelMask, inputChannel, inputSampleRate,
                numOutputChannels, 0, 0, 48000);

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        requestedInConfig.setInputPreset(StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION);
        requestedInConfig.setDeviceId(deviceId);
        requestedInConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        TestResult testResult = testCurrentConfigurations();
        if (testResult != null) {
            logOneLineSummary(testResult);
        }
    }

    void testInputDeviceCombo(int deviceId,
                              int deviceType,
                              int channelCount,
                              int channelMask,
                              int inputChannel,
                              int inputSampleRate) throws InterruptedException {
        if (NativeEngine.isMMapSupported()) {
            testInputDeviceCombo(deviceId, deviceType, channelCount, channelMask, inputChannel,
                    inputSampleRate, true);
        }
        testInputDeviceCombo(deviceId, deviceType, channelCount, channelMask, inputChannel,
                inputSampleRate, false);
    }

    void testInputDevices() throws InterruptedException {
        logBoth("\nTest Input Devices -------");

        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_INPUTS);
        int numTested = 0;
        for (AudioDeviceInfo deviceInfo : devices) {
            log("----\n"
                    + AudioDeviceInfoConverter.toString(deviceInfo));
            if (!deviceInfo.isSource()) continue; // FIXME log as error?!
            int deviceType = deviceInfo.getType();
            if (deviceType == AudioDeviceInfo.TYPE_BUILTIN_MIC) {
                int id = deviceInfo.getId();
                int[] channelCounts = deviceInfo.getChannelCounts();
                numTested++;
                // Always test mono and stereo.
                testInputDeviceCombo(id, deviceType, 1, 0, 0, 48000);
                testInputDeviceCombo(id, deviceType, 2, 0, 0, 48000);
                testInputDeviceCombo(id, deviceType, 2, 0, 1, 48000);
                if (channelCounts.length > 0) {
                    for (int numChannels : channelCounts) {
                        // Test higher channel counts.
                        if (numChannels > 2) {
                            log("numChannels = " + numChannels + "\n");
                            for (int channel = 0; channel < numChannels; channel++) {
                                testInputDeviceCombo(id, deviceType, numChannels, 0, channel,
                                        48000);
                            }
                        }
                    }
                }

                runOnUiThread(() -> mCheckBoxAllSampleRates.setEnabled(false));
                if (mCheckBoxAllSampleRates.isChecked()) {
                    for (int sampleRate : SAMPLE_RATES) {
                        testInputDeviceCombo(id, deviceType, 1, 0, 0, sampleRate);
                    }
                }

                if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2) {
                    int[] channelMasks = deviceInfo.getChannelMasks();
                    if (channelMasks.length > 0) {
                        for (int channelMask : channelMasks) {
                            int nativeChannelMask =
                                    convertJavaInChannelMaskToNativeChannelMask(channelMask);
                            if (nativeChannelMask == JAVA_CHANNEL_UNDEFINED) {
                                log("channelMask: " + channelMask + " not supported. Skipping.\n");
                                continue;
                            }
                            log("nativeChannelMask = " + convertChannelMaskToText(nativeChannelMask) + "\n");
                            int channelCount = Integer.bitCount(nativeChannelMask);
                            for (int channel = 0; channel < channelCount; channel++) {
                                testInputDeviceCombo(id, deviceType, channelCount, nativeChannelMask,
                                        channel, 48000);
                            }
                        }
                    }
                }
            } else {
                log("Device skipped. Not BuiltIn Mic.");
            }
        }

        if (numTested == 0) {
            log("NO INPUT DEVICE FOUND!\n");
        }
    }

    // The native out channel mask is its channel mask shifted right by 2 bits.
    // See AudioFormat.convertChannelOutMaskToNativeMask()
    int convertJavaOutChannelMaskToNativeChannelMask(int javaChannelMask) {
        return javaChannelMask >> 2;
    }

    // The native channel mask in AAudio is different than the Java in channel mask.
    // See AAudioConvert_aaudioToAndroidChannelLayoutMask()
    int convertJavaInChannelMaskToNativeChannelMask(int javaChannelMask) {
        switch (javaChannelMask) {
            case JAVA_CHANNEL_IN_MONO:
                return StreamConfiguration.CHANNEL_MONO;
            case JAVA_CHANNEL_IN_STEREO:
                return StreamConfiguration.CHANNEL_STEREO;
            case JAVA_CHANNEL_IN_FRONT_BACK:
                return StreamConfiguration.CHANNEL_FRONT_BACK;
            case JAVA_CHANNEL_IN_2POINT0POINT2:
                return StreamConfiguration.CHANNEL_2POINT0POINT2;
            case JAVA_CHANNEL_IN_2POINT1POINT2:
                return StreamConfiguration.CHANNEL_2POINT1POINT2;
            case JAVA_CHANNEL_IN_3POINT0POINT2:
                return StreamConfiguration.CHANNEL_3POINT0POINT2;
            case JAVA_CHANNEL_IN_3POINT1POINT2:
                return StreamConfiguration.CHANNEL_3POINT1POINT2;
            case JAVA_CHANNEL_IN_5POINT1:
                return StreamConfiguration.CHANNEL_5POINT1;
            default:
                log("Unimplemented java channel mask: " + javaChannelMask + "\n");
                return JAVA_CHANNEL_UNDEFINED;
        }
    }

    void logOneLineSummary(TestResult testResult) {
        logOneLineSummary(testResult, "");
    }

    void logOneLineSummary(TestResult testResult, String extra) {
        int result = testResult.result;
        String oneLineSummary;
        if (result == TEST_RESULT_SKIPPED) {
            oneLineSummary = "#" + mAutomatedTestRunner.getTestCount() + extra + ", SKIP";
        } else if (result == TEST_RESULT_FAILED) {
            oneLineSummary = getOneLineSummary() + extra + ", FAIL";
        } else {
            oneLineSummary = getOneLineSummary() + extra;
        }
        appendSummary(oneLineSummary + "\n");
    }

    void logBoth(String text) {
        log(text);
        appendSummary(text + "\n");
    }

    void logFailed(String text) {
        log(text);
        logAnalysis(text + "\n");
    }

     private void testDeviceOutputInfo(AudioDeviceInfo outputDeviceInfo)  throws InterruptedException {
         AudioDeviceInfo inputDeviceInfo = findCompatibleInputDevice(outputDeviceInfo.getType());
         showDeviceInfo(outputDeviceInfo, inputDeviceInfo);
         if (inputDeviceInfo == null) {
             return;
         }

         StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
         StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
         requestedInConfig.reset();
         requestedOutConfig.reset();
         requestedInConfig.setDeviceId(inputDeviceInfo.getId());
         requestedOutConfig.setDeviceId(outputDeviceInfo.getId());

         testOutputChannelCounts(inputDeviceInfo, outputDeviceInfo);

         runOnUiThread(() -> mCheckBoxAllSampleRates.setEnabled(false));
         if (mCheckBoxAllSampleRates.isChecked()) {
             for (int sampleRate : SAMPLE_RATES) {
                 requestedInConfig.setSampleRate(sampleRate);
                 requestedOutConfig.setSampleRate(sampleRate);
                 testPerformancePaths();
             }
         }
         requestedInConfig.setSampleRate(0);
         requestedOutConfig.setSampleRate(0);

         if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2
                 && isDeviceTypeMixed(outputDeviceInfo.getType())) {
             runOnUiThread(() -> mCheckBoxAllOutputChannelMasks.setEnabled(false));

             for (int channelMask : mCheckBoxAllOutputChannelMasks.isChecked() ?
                     ALL_OUTPUT_CHANNEL_MASKS : SHORT_OUTPUT_CHANNEL_MASKS) {
                 int channelCount = Integer.bitCount(channelMask);
                 requestedOutConfig.setChannelMask(channelMask);
                 for (int channel = 0; channel < channelCount; channel++) {
                     setOutputChannel(channel);
                     testPerformancePaths();
                 }
             }
         }
     }

    private void showDeviceInfo(AudioDeviceInfo outputDeviceInfo, AudioDeviceInfo inputDeviceInfo) {
        String deviceText = "OUT type = "
                + AudioDeviceInfoConverter.typeToString(outputDeviceInfo.getType());

        setInstructionsText(deviceText);

        if (inputDeviceInfo == null) {
            deviceText += "ERROR - cannot find compatible device type for input!";
        } else {
            deviceText = "IN type = "
                    + AudioDeviceInfoConverter.typeToString(inputDeviceInfo.getType())
                    + "\n" + deviceText;
            setInstructionsText(deviceText);
        }
    }

    private void testOutputChannelCounts(AudioDeviceInfo inputDeviceInfo, AudioDeviceInfo outputDeviceInfo) throws InterruptedException {
        ArrayList<Integer> channelCountsTested =new ArrayList<Integer>();
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        int[] outputChannelCounts = outputDeviceInfo.getChannelCounts();
        if (isDeviceTypeMixed(outputDeviceInfo.getType())) {
            requestedInConfig.setChannelCount(1);
            setInputChannel(0);
            // test mono
            requestedOutConfig.setChannelCount(1);
            channelCountsTested.add(1);
            setOutputChannel(0);
            testPerformancePaths();
            // test stereo
            requestedOutConfig.setChannelCount(2);
            channelCountsTested.add(2);
            setOutputChannel(0);
            testPerformancePaths();
            setOutputChannel(1);
            testPerformancePaths();
            // Test channels for each channelCount above 2
            for (int numChannels : outputChannelCounts) {
                log("numChannels = " + numChannels);
                if (numChannels > 4) {
                    log("numChannels forced to 4!");
                }
                if (!channelCountsTested.contains(numChannels)) {
                    requestedOutConfig.setChannelCount(numChannels);
                    channelCountsTested.add(numChannels);
                    for (int channel = 0; channel < numChannels; channel++) {
                        setOutputChannel(channel);
                        testPerformancePaths();
                    }
                }
            }
        } else {
            // test mono
            testMatchingChannels(1);
            channelCountsTested.add(1);
            // Test two matching stereo channels.
            testMatchingChannels(2);
            channelCountsTested.add(2);
            // Test matching channels for each channelCount above 2
            for (int numChannels : outputChannelCounts) {
                log("numChannels = " + numChannels);
                if (numChannels > 4) {
                    log("numChannels forced to 4!");
                    numChannels = 4;
                }
                if (!channelCountsTested.contains(numChannels)) {
                    testMatchingChannels(numChannels);
                    channelCountsTested.add(numChannels);
                }
            }
        }
        // Restore defaults.
        requestedInConfig.setChannelCount(1);
        setInputChannel(0);
        requestedOutConfig.setChannelCount(1);
        setOutputChannel(0);
    }

    private void testMatchingChannels(int numChannels) throws InterruptedException {
        mAudioInputTester.requestedConfiguration.setChannelCount(numChannels);
        mAudioOutTester.requestedConfiguration.setChannelCount(numChannels);
        for (int channel = 0; channel < numChannels; channel++) {
            setInputChannel(channel);
            setOutputChannel(channel);
            testPerformancePaths();
        }
    }

    private void testPerformancePaths() throws InterruptedException {
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
        requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);

        requestedInConfig.setMMap(false);
        requestedOutConfig.setMMap(false);
        testCurrentConfigurations();

        if (NativeEngine.isMMapSupported()) {
            requestedInConfig.setMMap(true);
            requestedOutConfig.setMMap(true);
            testCurrentConfigurations();
        }
        requestedInConfig.setMMap(false);
        requestedOutConfig.setMMap(false);

        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);
        testCurrentConfigurations();
    }

    private void testOutputDeviceTypes()  throws InterruptedException {
        // Determine which output device type to test based on priorities.
        AudioDeviceInfo info = getDeviceInfoByType(AudioDeviceInfo.TYPE_USB_DEVICE,
                AudioManager.GET_DEVICES_OUTPUTS);
        if (info != null) {
            testDeviceOutputInfo(info);
            return;
        }
        info = getDeviceInfoByType(AudioDeviceInfo.TYPE_USB_HEADSET,
                AudioManager.GET_DEVICES_OUTPUTS);
        if (info != null) {
            testDeviceOutputInfo(info);
            return;
        }
        info = getDeviceInfoByType(AudioDeviceInfo.TYPE_WIRED_HEADSET,
                AudioManager.GET_DEVICES_OUTPUTS);
        if (info != null) {
            testDeviceOutputInfo(info);
            return;
        }
        // Test both SPEAKER and SPEAKER_SAFE
        info = getDeviceInfoByType(AudioDeviceInfo.TYPE_BUILTIN_SPEAKER,
                AudioManager.GET_DEVICES_OUTPUTS);
        if (info != null) {
            testDeviceOutputInfo(info);
            // Continue on to SPEAKER_SAFE
        }
        info = getDeviceInfoByType(AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE,
                AudioManager.GET_DEVICES_OUTPUTS);
        if (info != null) {
            testDeviceOutputInfo(info);
        }
    }

    @Override
    public void runTest() {
        try {
            logDeviceInfo();
            log("min.required.magnitude = " + MIN_REQUIRED_MAGNITUDE);
            log("max.allowed.jitter = " + MAX_ALLOWED_JITTER);
            log("test.gap.msec = " + mGapMillis);
            
            mTestResults.clear();
            mDurationSeconds = DURATION_SECONDS;

            runOnUiThread(() -> keepScreenOn(true));

            testOutputDeviceTypes();

//            if (mCheckBoxInputPresets.isChecked()) {
//                runOnUiThread(() -> mCheckBoxInputPresets.setEnabled(false));
//                testInputPresets();
//            }
//            if (mCheckBoxInputDevices.isChecked()) {
//                runOnUiThread(() -> mCheckBoxInputDevices.setEnabled(false));
//                testInputDevices();
//            }
//            if (mCheckBoxOutputDevices.isChecked()) {
//                runOnUiThread(() -> mCheckBoxOutputDevices.setEnabled(false));
//                testOutputDevices();
//            }

            compareFailedTestsWithNearestPassingTest();

        } catch (InterruptedException e) {
            compareFailedTestsWithNearestPassingTest();
        } catch (Exception e) {
            log(e.getMessage());
            showErrorToast(e.getMessage());
        } finally {
            runOnUiThread(() -> {
                mCheckBoxInputPresets.setEnabled(true);
                mCheckBoxInputDevices.setEnabled(true);
                mCheckBoxOutputDevices.setEnabled(true);
                mCheckBoxAllOutputChannelMasks.setEnabled(true);
                mCheckBoxAllSampleRates.setEnabled(true);
                keepScreenOn(false);
            });
        }
    }

    @Override
    public void startTestUsingBundle() {
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        configureStreamsFromBundle(mBundleFromIntent, requestedInConfig, requestedOutConfig);

        boolean shouldUseInputPresets = mBundleFromIntent.getBoolean(KEY_USE_INPUT_PRESETS,
                VALUE_DEFAULT_USE_INPUT_PRESETS);
        boolean shouldUseInputDevices = mBundleFromIntent.getBoolean(KEY_USE_INPUT_DEVICES,
                VALUE_DEFAULT_USE_INPUT_DEVICES);
        boolean shouldUseOutputDevices = mBundleFromIntent.getBoolean(KEY_USE_OUTPUT_DEVICES,
                VALUE_DEFAULT_USE_OUTPUT_DEVICES);
        boolean shouldUseAllOutputChannelMasks =
                mBundleFromIntent.getBoolean(KEY_USE_ALL_OUTPUT_CHANNEL_MASKS,
                VALUE_DEFAULT_USE_ALL_OUTPUT_CHANNEL_MASKS);
        boolean shouldUseAllSampleRates =
                mBundleFromIntent.getBoolean(KEY_USE_ALL_SAMPLE_RATES,
                        VALUE_DEFAULT_USE_ALL_SAMPLE_RATES);
        int singleTestIndex = mBundleFromIntent.getInt(KEY_SINGLE_TEST_INDEX,
                VALUE_DEFAULT_SINGLE_TEST_INDEX);

        runOnUiThread(() -> {
            mCheckBoxInputPresets.setChecked(shouldUseInputPresets);
            mCheckBoxInputDevices.setChecked(shouldUseInputDevices);
            mCheckBoxOutputDevices.setChecked(shouldUseOutputDevices);
            mCheckBoxAllOutputChannelMasks.setChecked(shouldUseAllOutputChannelMasks);
            mCheckBoxAllSampleRates.setChecked(shouldUseAllSampleRates);
            mAutomatedTestRunner.setTestIndexText(singleTestIndex);
        });

        mAutomatedTestRunner.startTest();
    }
}
