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

import android.content.Context;
import android.media.AudioManager;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;
import android.widget.LinearLayout;

import com.google.sample.audio_device.AudioDeviceListEntry;
import com.google.sample.audio_device.AudioDeviceSpinner;

import java.text.BreakIterator;

/**
 * View for Editing a requested StreamConfiguration
 * and displaying the actual StreamConfiguration.
 */

public class StreamConfigurationView extends LinearLayout {

    private StreamConfiguration  mRequestedConfiguration;
    private StreamConfiguration  mActualConfiguration;

    protected Spinner mNativeApiSpinner;
    private TextView mActualNativeApiView;

    private TextView mActualMMapView;
    private CheckBox mRequestedMMapView;
    private TextView mActualExclusiveView;
    private TextView mActualPerformanceView;
    private Spinner  mPerformanceSpinner;
    private CheckBox mRequestedExclusiveView;
    private CheckBox mChannelConversionBox;
    private CheckBox mFormatConversionBox;
    private Spinner  mChannelCountSpinner;
    private TextView mActualChannelCountView;
    private TextView mActualFormatView;

    private TextView mActualInputPresetView;
    private Spinner  mInputPresetSpinner;
    private TableRow mInputPresetTableRow;
    private Spinner  mFormatSpinner;
    private Spinner  mSampleRateSpinner;
    private Spinner  mRateConversionQualitySpinner;
    private TextView mActualSampleRateView;
    private LinearLayout mHideableView;

    private AudioDeviceSpinner mDeviceSpinner;
    private TextView mActualSessionIdView;
    private CheckBox mRequestAudioEffect;

    private TextView mStreamInfoView;
    private TextView mStreamStatusView;
    private TextView mOptionExpander;
    private String mHideSettingsText;
    private String mShowSettingsText;

    // Create an anonymous implementation of OnClickListener
    private View.OnClickListener mToggleListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mHideableView.isShown()) {
                hideSettingsView();
            } else {
                showSettingsView();
            }
        }
    };

    public static String yesOrNo(boolean b) {
        return b ?  "YES" : "NO";
    }

    private void updateSettingsViewText() {
        if (mHideableView.isShown()) {
            mOptionExpander.setText(mHideSettingsText);
        } else {
            mOptionExpander.setText(mShowSettingsText);
        }
    }

    public void showSettingsView() {
        mHideableView.setVisibility(View.VISIBLE);
        updateSettingsViewText();
    }

    public void hideSampleRateMenu() {
        if (mSampleRateSpinner != null) {
            mSampleRateSpinner.setVisibility(View.GONE);
        }
    }

    public void hideSettingsView() {
        mHideableView.setVisibility(View.GONE);
        updateSettingsViewText();
    }

    public StreamConfigurationView(Context context) {
        super(context);
        initializeViews(context);
    }

    public StreamConfigurationView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public StreamConfigurationView(Context context,
                                   AttributeSet attrs,
                                   int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    /**
     * Inflates the views in the layout.
     *
     * @param context
     *           the current context for the view.
     */
    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.stream_config, this);

        mHideSettingsText = getResources().getString(R.string.hint_hide_settings);
        mShowSettingsText = getResources().getString(R.string.hint_show_settings);

        mHideableView = (LinearLayout) findViewById(R.id.hideableView);

        mOptionExpander = (TextView) findViewById(R.id.toggle_stream_config);
        mOptionExpander.setOnClickListener(mToggleListener);

        mNativeApiSpinner = (Spinner) findViewById(R.id.spinnerNativeApi);
        mNativeApiSpinner.setOnItemSelectedListener(new NativeApiSpinnerListener());
        mNativeApiSpinner.setSelection(StreamConfiguration.NATIVE_API_UNSPECIFIED);

        mActualNativeApiView = (TextView) findViewById(R.id.actualNativeApi);

        mChannelConversionBox = (CheckBox) findViewById(R.id.checkChannelConversion);
        mChannelConversionBox.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mRequestedConfiguration.setChannelConversionAllowed(mChannelConversionBox.isChecked());
            }
        });

        mFormatConversionBox = (CheckBox) findViewById(R.id.checkFormatConversion);
        mFormatConversionBox.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mRequestedConfiguration.setFormatConversionAllowed(mFormatConversionBox.isChecked());
            }
        });

        mActualMMapView = (TextView) findViewById(R.id.actualMMap);
        mRequestedMMapView = (CheckBox) findViewById(R.id.requestedMMapEnable);
        mRequestedMMapView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mRequestedConfiguration.setMMap(mRequestedMMapView.isChecked());
            }
        });
        boolean mmapSupported = NativeEngine.isMMapSupported();
        mRequestedMMapView.setEnabled(mmapSupported);
        mRequestedMMapView.setChecked(mmapSupported);

        mActualExclusiveView = (TextView) findViewById(R.id.actualExclusiveMode);
        mRequestedExclusiveView = (CheckBox) findViewById(R.id.requestedExclusiveMode);
        mRequestedExclusiveView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mRequestedConfiguration.setSharingMode(mRequestedExclusiveView.isChecked()
                        ? StreamConfiguration.SHARING_MODE_EXCLUSIVE
                        : StreamConfiguration.SHARING_MODE_SHARED);
            }
        });

        boolean mmapExclusiveSupported = NativeEngine.isMMapExclusiveSupported();
        mRequestedExclusiveView.setEnabled(mmapExclusiveSupported);
        mRequestedExclusiveView.setChecked(mmapExclusiveSupported);

        mActualSessionIdView = (TextView) findViewById(R.id.sessionId);
        mRequestAudioEffect = (CheckBox) findViewById(R.id.requestAudioEffect);
        mRequestAudioEffect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mRequestedConfiguration.setSessionId(mRequestAudioEffect.isChecked()
                        ? StreamConfiguration.SESSION_ID_ALLOCATE
                        : StreamConfiguration.SESSION_ID_NONE);
            }
        });

        mActualSampleRateView = (TextView) findViewById(R.id.actualSampleRate);
        mSampleRateSpinner = (Spinner) findViewById(R.id.spinnerSampleRate);
        mSampleRateSpinner.setOnItemSelectedListener(new SampleRateSpinnerListener());

        mActualChannelCountView = (TextView) findViewById(R.id.actualChannelCount);
        mChannelCountSpinner = (Spinner) findViewById(R.id.spinnerChannelCount);
        mChannelCountSpinner.setOnItemSelectedListener(new ChannelCountSpinnerListener());

        mActualFormatView = (TextView) findViewById(R.id.actualAudioFormat);
        mFormatSpinner = (Spinner) findViewById(R.id.spinnerFormat);
        mFormatSpinner.setOnItemSelectedListener(new FormatSpinnerListener());

        mRateConversionQualitySpinner = (Spinner) findViewById(R.id.spinnerSRCQuality);
        mRateConversionQualitySpinner.setOnItemSelectedListener(new RateConversionQualitySpinnerListener());

        mActualPerformanceView = (TextView) findViewById(R.id.actualPerformanceMode);
        mPerformanceSpinner = (Spinner) findViewById(R.id.spinnerPerformanceMode);
        mPerformanceSpinner.setOnItemSelectedListener(new PerformanceModeSpinnerListener());
        mPerformanceSpinner.setSelection(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY
                - StreamConfiguration.PERFORMANCE_MODE_NONE);

        mInputPresetTableRow = (TableRow) findViewById(R.id.rowInputPreset);
        mActualInputPresetView = (TextView) findViewById(R.id.actualInputPreset);
        mInputPresetSpinner = (Spinner) findViewById(R.id.spinnerInputPreset);
        mInputPresetSpinner.setOnItemSelectedListener(new InputPresetSpinnerListener());
        mInputPresetSpinner.setSelection(2); // TODO need better way to select voice recording default

        mStreamInfoView = (TextView) findViewById(R.id.streamInfo);

        mStreamStatusView = (TextView) findViewById(R.id.statusView);

        mDeviceSpinner = (AudioDeviceSpinner) findViewById(R.id.devices_spinner);
        mDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                int id =  ((AudioDeviceListEntry) mDeviceSpinner.getSelectedItem()).getId();
                mRequestedConfiguration.setDeviceId(id);
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                mRequestedConfiguration.setDeviceId(StreamConfiguration.UNSPECIFIED);
            }
        });

        showSettingsView();
    }

    public void setOutput(boolean output) {
        String ioText;
        if (output) {
            mDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_OUTPUTS);
            ioText = "OUTPUT";
        } else {
            mDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_INPUTS);
            ioText = "INPUT";
        }
        mHideSettingsText = getResources().getString(R.string.hint_hide_settings) + " - " + ioText;
        mShowSettingsText = getResources().getString(R.string.hint_show_settings) + " - " + ioText;
        updateSettingsViewText();

        // Don't show InputPresets for output streams.
        mInputPresetTableRow.setVisibility(output ? View.GONE : View.VISIBLE);
    }

    private class NativeApiSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            mRequestedConfiguration.setNativeApi(pos);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setNativeApi(StreamConfiguration.NATIVE_API_UNSPECIFIED);
        }
    }

    private class PerformanceModeSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int performanceMode, long id) {
            mRequestedConfiguration.setPerformanceMode(performanceMode
                    + StreamConfiguration.PERFORMANCE_MODE_NONE);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);
        }
    }

    private class ChannelCountSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            mRequestedConfiguration.setChannelCount(pos);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setChannelCount(StreamConfiguration.UNSPECIFIED);
        }
    }

    private class SampleRateSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            String text = parent.getItemAtPosition(pos).toString();
            int sampleRate = Integer.parseInt(text);
            mRequestedConfiguration.setSampleRate(sampleRate);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setSampleRate(StreamConfiguration.UNSPECIFIED);
        }
    }

    private class FormatSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            // Menu position matches actual enum value!
            mRequestedConfiguration.setFormat(pos);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setFormat(StreamConfiguration.UNSPECIFIED);
        }
    }

    private class InputPresetSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            String text = parent.getItemAtPosition(pos).toString();
            int inputPreset = StreamConfiguration.convertTextToInputPreset(text);
            mRequestedConfiguration.setInputPreset(inputPreset);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setInputPreset(StreamConfiguration.INPUT_PRESET_GENERIC);
        }
    }

    private class RateConversionQualitySpinnerListener
            implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            // Menu position matches actual enum value!
            mRequestedConfiguration.setRateConversionQuality(pos);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mRequestedConfiguration.setRateConversionQuality(StreamConfiguration.RATE_CONVERSION_QUALITY_HIGH);
        }
    }

    public void setChildrenEnabled(boolean enabled) {
        mNativeApiSpinner.setEnabled(enabled);
        mPerformanceSpinner.setEnabled(enabled);
        mRequestedExclusiveView.setEnabled(enabled);
        mSampleRateSpinner.setEnabled(enabled);
        mChannelCountSpinner.setEnabled(enabled);
        mFormatSpinner.setEnabled(enabled);
        mDeviceSpinner.setEnabled(enabled);
        mRequestAudioEffect.setEnabled(enabled);
    }

    // This must be called on the UI thread.
    void updateDisplay() {
        int value;

        value = mActualConfiguration.getNativeApi();
        mActualNativeApiView.setText(StreamConfiguration.convertNativeApiToText(value));

        mActualMMapView.setText(yesOrNo(mActualConfiguration.isMMap()));
        int sharingMode = mActualConfiguration.getSharingMode();
        boolean isExclusive = (sharingMode == StreamConfiguration.SHARING_MODE_EXCLUSIVE);
        mActualExclusiveView.setText(yesOrNo(isExclusive));

        value = mActualConfiguration.getPerformanceMode();
        mActualPerformanceView.setText(StreamConfiguration.convertPerformanceModeToText(value));
        mActualPerformanceView.requestLayout();

        value = mActualConfiguration.getFormat();
        mActualFormatView.setText(StreamConfiguration.convertFormatToText(value));
        mActualFormatView.requestLayout();

        value = mActualConfiguration.getInputPreset();
        mActualInputPresetView.setText(StreamConfiguration.convertInputPresetToText(value));
        mActualInputPresetView.requestLayout();

        mActualChannelCountView.setText(mActualConfiguration.getChannelCount() + "");
        mActualSampleRateView.setText(mActualConfiguration.getSampleRate() + "");
        mActualSessionIdView.setText("S#: " + mActualConfiguration.getSessionId());

        boolean isMMap = mActualConfiguration.isMMap();
        mStreamInfoView.setText("burst = " + mActualConfiguration.getFramesPerBurst()
                + ", capacity = " + mActualConfiguration.getBufferCapacityInFrames()
                + ", devID = " + mActualConfiguration.getDeviceId()
                + ", " + (mActualConfiguration.isMMap() ? "MMAP" : "Legacy")
                + (isMMap ? ", " + StreamConfiguration.convertSharingModeToText(sharingMode) : "")
        );

        mHideableView.requestLayout();
    }

    // This must be called on the UI thread.
    public void setStatusText(String msg) {
        mStreamStatusView.setText(msg);
    }

    protected StreamConfiguration getRequestedConfiguration() {
        return mRequestedConfiguration;
    }

    public void setRequestedConfiguration(StreamConfiguration configuration) {
        mRequestedConfiguration = configuration;
        if (configuration != null) {
            mRateConversionQualitySpinner.setSelection(configuration.getRateConversionQuality());
            mChannelConversionBox.setChecked(configuration.getChannelConversionAllowed());
            mFormatConversionBox.setChecked(configuration.getFormatConversionAllowed());
        }
    }

    protected StreamConfiguration getActualConfiguration() {
        return mActualConfiguration;
    }
    public void setActualConfiguration(StreamConfiguration configuration) {
        mActualConfiguration = configuration;
    }

    public void setExclusiveMode(boolean b) {
        mRequestedExclusiveView.setChecked(b);
        mRequestedConfiguration.setSharingMode(b
                ? StreamConfiguration.SHARING_MODE_EXCLUSIVE
                : StreamConfiguration.SHARING_MODE_SHARED);
    }

    public void setFormat(int format) {
        mFormatSpinner.setSelection(format); // position matches format
        mRequestedConfiguration.setFormat(format);
    }

    public void setFormatConversionAllowed(boolean allowed) {
        mFormatConversionBox.setChecked(allowed);
        mRequestedConfiguration.setFormatConversionAllowed(allowed);
    }
}
