package com.google.sample.oboe.manualtest;

import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Spinner;

public class AutomatedGlitchActivity  extends BaseAutoGlitchActivity {

    private Spinner mDurationSpinner;

    // Test with these configurations.
    private static final int[] PERFORMANCE_MODES = {
            StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
            StreamConfiguration.PERFORMANCE_MODE_NONE
    };
    private static final int[] SAMPLE_RATES = { 48000, 44100, 16000 };
    private static final int MONO = 1;
    private static final int STEREO = 2;
    private static final int UNSPECIFIED = 0;

    private class DurationSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            String text = parent.getItemAtPosition(pos).toString();
            mDurationSeconds = Integer.parseInt(text);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mDurationSeconds = DEFAULT_DURATION_SECONDS;
        }
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_auto_glitches);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mDurationSpinner = (Spinner) findViewById(R.id.spinner_glitch_duration);
        mDurationSpinner.setOnItemSelectedListener(new DurationSpinnerListener());
    }

    @Override
    public String getTestName() {
        return "AutoGlitch";
    }

    private void testConfiguration(int perfMode,
                                   int sharingMode,
                                   int sampleRate,
                                   int inChannels,
                                   int outChannels) throws InterruptedException {

        // Configure settings
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        requestedInConfig.reset();
        requestedOutConfig.reset();

        requestedInConfig.setPerformanceMode(perfMode);
        requestedOutConfig.setPerformanceMode(perfMode);

        requestedInConfig.setSharingMode(sharingMode);
        requestedOutConfig.setSharingMode(sharingMode);

        requestedInConfig.setSampleRate(sampleRate);
        requestedOutConfig.setSampleRate(sampleRate);

        requestedInConfig.setChannelCount(inChannels);
        requestedOutConfig.setChannelCount(outChannels);

        setTolerance(0.3f); // FIXME remove

        testConfigurations();
    }

    private void testConfiguration(int performanceMode,
                                   int sharingMode,
                                   int sampleRate) throws InterruptedException {
        testConfiguration(performanceMode,
                sharingMode,
                sampleRate, MONO, STEREO);
        testConfiguration(performanceMode,
                sharingMode,
                sampleRate, STEREO, MONO);
    }

    @Override
    public void runTest() {
        try {
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_EXCLUSIVE,
                    UNSPECIFIED);

            for (int perfMode : PERFORMANCE_MODES) {
                for (int sampleRate : SAMPLE_RATES) {
                    testConfiguration(perfMode,
                            StreamConfiguration.SHARING_MODE_SHARED,
                            sampleRate);
                }
            }
        } catch (InterruptedException e) {
            log(e.getMessage());
            showErrorToast(e.getMessage());
        }
    }

}
