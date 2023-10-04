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

import android.app.Activity;
import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.CheckBox;
import androidx.annotation.NonNull;

import com.mobileer.audio_device.AudioDeviceInfoConverter;

import java.lang.reflect.Field;
import java.util.Locale;

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

    public static final String KEY_USE_INPUT_PRESETS = "use_input_presets";
    public static final boolean VALUE_DEFAULT_USE_INPUT_PRESETS = true;

    public static final String KEY_USE_INPUT_DEVICES = "use_input_devices";
    public static final boolean VALUE_DEFAULT_USE_INPUT_DEVICES = true;

    public static final String KEY_USE_OUTPUT_DEVICES = "use_output_devices";
    public static final boolean VALUE_DEFAULT_USE_OUTPUT_DEVICES = true;

    public static final String KEY_USE_ALL_OUTPUT_CHANNEL_MASKS = "use_all_output_channel_masks";
    public static final boolean VALUE_DEFAULT_USE_ALL_OUTPUT_CHANNEL_MASKS = false;

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

    AudioManager   mAudioManager;
    private CheckBox mCheckBoxInputPresets;
    private CheckBox mCheckBoxInputDevices;
    private CheckBox mCheckBoxOutputDevices;
    private CheckBox mCheckBoxAllOutputChannelMasks;

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
            StreamConfiguration.CHANNEL_2POINT1,
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
                double phase = getPhase();
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
    native double getPhase();

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_data_paths);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        mCheckBoxInputPresets = (CheckBox)findViewById(R.id.checkbox_paths_input_presets);
        mCheckBoxInputDevices = (CheckBox)findViewById(R.id.checkbox_paths_input_devices);
        mCheckBoxOutputDevices = (CheckBox)findViewById(R.id.checkbox_paths_output_devices);
        mCheckBoxAllOutputChannelMasks =
                (CheckBox)findViewById(R.id.checkbox_paths_all_output_channel_masks);
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
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        // No point running the test if we don't get the data path we requested.
        if (actualInConfig.isMMap() != requestedInConfig.isMMap()) {
            log("Did not get requested MMap input stream");
            why += "mmap";
        }
        if (actualOutConfig.isMMap() != requestedOutConfig.isMMap()) {
            log("Did not get requested MMap output stream");
            why += "mmap";
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
        if ((requestedInConfig.getInputPreset() != actualInConfig.getInputPreset())) {
            why += ", inPre(" + requestedInConfig.getInputPreset()
                    + "!=" + actualInConfig.getInputPreset() + ")";
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
                + ", OUT" + (actualOutConfig.isMMap() ? "-M" : "-L")
                + " D=" + actualOutConfig.getDeviceId()
                + ", ch=" + channelText(getOutputChannel(), actualOutConfig.getChannelCount())
                + ", mag = " + getMagnitudeText(mMaxMagnitude);
    }

    void setupDeviceCombo(int inputChannelCount,
                          int inputChannelMask,
                          int inputChannel,
                          int outputChannelCount,
                          int outputChannelMask,
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

    private TestResult testConfigurationsAddMagJitter() throws InterruptedException {
        TestResult testResult = testInOutConfigurations();
        if (testResult != null) {
            testResult.addComment("mag = " + TestDataPathsActivity.getMagnitudeText(mMagnitude)
                    + ", jitter = " + getJitterText());
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
        setupDeviceCombo(numInputChannels, 0, inputChannel, numOutputChannels, 0,
                outputChannel);

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        requestedInConfig.setInputPreset(inputPreset);
        requestedInConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        TestResult testResult = testConfigurationsAddMagJitter();
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
                              boolean mmapEnabled) throws InterruptedException {
        String typeString = AudioDeviceInfoConverter.typeToString(deviceType);
        if (channelMask != 0) {
            setTestName("Test InDev: #" + deviceId + " " + typeString + "_" +
                    convertChannelMaskToText(channelMask) + "_" +
                    inputChannel + "/" + channelCount);
        } else {
            setTestName("Test InDev: #" + deviceId + " " + typeString
                    + "_" + inputChannel + "/" + channelCount);
        }

        final int numOutputChannels = 2;
        setupDeviceCombo(channelCount, channelMask, inputChannel, numOutputChannels, 0,
                0);

        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        requestedInConfig.setInputPreset(StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION);
        requestedInConfig.setDeviceId(deviceId);
        requestedInConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        TestResult testResult = testConfigurationsAddMagJitter();
        if (testResult != null) {
            logOneLineSummary(testResult);
        }
    }

    void testInputDeviceCombo(int deviceId,
                              int deviceType,
                              int channelCount,
                              int channelMask,
                              int inputChannel) throws InterruptedException {
        if (NativeEngine.isMMapSupported()) {
            testInputDeviceCombo(deviceId, deviceType, channelCount, channelMask, inputChannel,
                    true);
        }
        testInputDeviceCombo(deviceId, deviceType, channelCount, channelMask, inputChannel,
                false);
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
                testInputDeviceCombo(id, deviceType, 1, 0, 0);
                testInputDeviceCombo(id, deviceType, 2, 0, 0);
                testInputDeviceCombo(id, deviceType, 2, 0, 1);
                if (channelCounts.length > 0) {
                    for (int numChannels : channelCounts) {
                        // Test higher channel counts.
                        if (numChannels > 2) {
                            log("numChannels = " + numChannels + "\n");
                            for (int channel = 0; channel < numChannels; channel++) {
                                testInputDeviceCombo(id, deviceType, numChannels, 0, channel);
                            }
                        }
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
                                        channel);
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

    void testOutputDeviceCombo(int deviceId,
                               int deviceType,
                               int channelCount,
                               int channelMask,
                               int outputChannel,
                               boolean mmapEnabled) throws InterruptedException {
        String typeString = AudioDeviceInfoConverter.typeToString(deviceType);
        if (channelMask != 0) {
            setTestName("Test OutDev: #" + deviceId + " " + typeString
                    + " Mask:" + channelMask + "_" + outputChannel + "/" + channelCount);
        } else {
            setTestName("Test InDev: #" + deviceId + " " + typeString
                    + "_" + outputChannel + "/" + channelCount);
        }

        final int numInputChannels = 2; // TODO review, done because of mono problems on some devices
        setupDeviceCombo(numInputChannels, 0, 0, channelCount, channelMask,
                outputChannel);

        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        requestedOutConfig.setDeviceId(deviceId);
        requestedOutConfig.setMMap(mmapEnabled);

        mMagnitude = -1.0;
        TestResult testResult = testConfigurationsAddMagJitter();
        if (testResult != null) {
            logOneLineSummary(testResult);
            int result = testResult.result;
            if (result == TEST_RESULT_FAILED) {
                if (deviceType == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE
                        && channelCount == 2
                        && outputChannel == 1) {
                    testResult.addComment("Maybe EARPIECE does not mix stereo to mono!");
                }
                if (deviceType == TYPE_BUILTIN_SPEAKER_SAFE
                        && channelCount == 2
                        && outputChannel == 0) {
                    testResult.addComment("Maybe SPEAKER_SAFE dropped channel zero!");
                }
            }
        }
    }

    void testOutputDeviceCombo(int deviceId,
                               int deviceType,
                               int channelCount,
                               int channelMask,
                               int outputChannel) throws InterruptedException {
        if (NativeEngine.isMMapSupported()) {
            testOutputDeviceCombo(deviceId, deviceType, channelCount, channelMask, outputChannel,
                    true);
        }
        testOutputDeviceCombo(deviceId, deviceType, channelCount, channelMask, outputChannel
                , false);
    }

    void logBoth(String text) {
        log(text);
        appendSummary(text + "\n");
    }

    void logFailed(String text) {
        log(text);
        logAnalysis(text + "\n");
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
                || deviceType == TYPE_BUILTIN_SPEAKER_SAFE) {
                int id = deviceInfo.getId();
                int[] channelCounts = deviceInfo.getChannelCounts();
                numTested++;
                // Always test mono and stereo.
                testOutputDeviceCombo(id, deviceType, 1, 0, 0);
                testOutputDeviceCombo(id, deviceType, 2, 0, 0);
                testOutputDeviceCombo(id, deviceType, 2, 0, 1);
                if (channelCounts.length > 0) {
                    for (int numChannels : channelCounts) {
                        // Test higher channel counts.
                        if (numChannels > 2) {
                            log("numChannels = " + numChannels + "\n");
                            for (int channel = 0; channel < numChannels; channel++) {
                                testOutputDeviceCombo(id, deviceType, numChannels, 0, channel);
                            }
                        }
                    }
                }

                if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2
                        && deviceType == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
                    runOnUiThread(() -> mCheckBoxAllOutputChannelMasks.setEnabled(false));

                    for (int channelMask : mCheckBoxAllOutputChannelMasks.isChecked() ?
                            ALL_OUTPUT_CHANNEL_MASKS : SHORT_OUTPUT_CHANNEL_MASKS) {
                        log("channelMask = " + convertChannelMaskToText(channelMask) + "\n");
                        int channelCount = Integer.bitCount(channelMask);
                        for (int channel = 0; channel < channelCount; channel++) {
                            testOutputDeviceCombo(id, deviceType, channelCount, channelMask, channel);
                        }
                    }
                }
            } else {
                log("Device skipped because DeviceType is not testable.");
            }
        }
        if (numTested == 0) {
            log("NO OUTPUT DEVICE FOUND!\n");
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

            if (mCheckBoxInputPresets.isChecked()) {
                runOnUiThread(() -> mCheckBoxInputPresets.setEnabled(false));
                testInputPresets();
            }
            if (mCheckBoxInputDevices.isChecked()) {
                runOnUiThread(() -> mCheckBoxInputDevices.setEnabled(false));
                testInputDevices();
            }
            if (mCheckBoxOutputDevices.isChecked()) {
                runOnUiThread(() -> mCheckBoxOutputDevices.setEnabled(false));
                testOutputDevices();
            }

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
        int singleTestIndex = mBundleFromIntent.getInt(KEY_SINGLE_TEST_INDEX,
                VALUE_DEFAULT_SINGLE_TEST_INDEX);

        runOnUiThread(() -> {
            mCheckBoxInputPresets.setChecked(shouldUseInputPresets);
            mCheckBoxInputDevices.setChecked(shouldUseInputDevices);
            mCheckBoxOutputDevices.setChecked(shouldUseOutputDevices);
            mCheckBoxAllOutputChannelMasks.setChecked(shouldUseAllOutputChannelMasks);
            mAutomatedTestRunner.setTestIndexText(singleTestIndex);
        });

        mAutomatedTestRunner.startTest();
    }
}
