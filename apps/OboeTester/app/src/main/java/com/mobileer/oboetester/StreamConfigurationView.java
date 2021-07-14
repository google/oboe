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

package com.mobileer.oboetester;

import android.content.Context;
import android.media.AudioManager;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TableRow;
import android.widget.TextView;
import android.widget.LinearLayout;

import com.mobileer.audio_device.AudioDeviceListEntry;
import com.mobileer.audio_device.AudioDeviceSpinner;

/**
 * View for Editing a requested StreamConfiguration
 * and displaying the actual StreamConfiguration.
 */

public class StreamConfigurationView extends LinearLayout {
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

    private TableRow mInputPresetTableRow;
    private Spinner  mInputPresetSpinner;
    private TextView mActualInputPresetView;

    private TableRow mUsageTableRow;
    private Spinner  mUsageSpinner;
    private TextView mActualUsageView;

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
        mNativeApiSpinner.setSelection(StreamConfiguration.NATIVE_API_UNSPECIFIED);

        mActualNativeApiView = (TextView) findViewById(R.id.actualNativeApi);

        mChannelConversionBox = (CheckBox) findViewById(R.id.checkChannelConversion);

        mFormatConversionBox = (CheckBox) findViewById(R.id.checkFormatConversion);

        mActualMMapView = (TextView) findViewById(R.id.actualMMap);
        mRequestedMMapView = (CheckBox) findViewById(R.id.requestedMMapEnable);
        boolean mmapSupported = NativeEngine.isMMapSupported();
        mRequestedMMapView.setEnabled(mmapSupported);
        mRequestedMMapView.setChecked(mmapSupported);

        mActualExclusiveView = (TextView) findViewById(R.id.actualExclusiveMode);
        mRequestedExclusiveView = (CheckBox) findViewById(R.id.requestedExclusiveMode);

        boolean mmapExclusiveSupported = NativeEngine.isMMapExclusiveSupported();
        mRequestedExclusiveView.setEnabled(mmapExclusiveSupported);
        mRequestedExclusiveView.setChecked(mmapExclusiveSupported);

        mActualSessionIdView = (TextView) findViewById(R.id.sessionId);
        mRequestAudioEffect = (CheckBox) findViewById(R.id.requestAudioEffect);

        mActualSampleRateView = (TextView) findViewById(R.id.actualSampleRate);
        mSampleRateSpinner = (Spinner) findViewById(R.id.spinnerSampleRate);
        mActualChannelCountView = (TextView) findViewById(R.id.actualChannelCount);
        mChannelCountSpinner = (Spinner) findViewById(R.id.spinnerChannelCount);
        mActualFormatView = (TextView) findViewById(R.id.actualAudioFormat);
        mFormatSpinner = (Spinner) findViewById(R.id.spinnerFormat);
        mRateConversionQualitySpinner = (Spinner) findViewById(R.id.spinnerSRCQuality);

        mActualPerformanceView = (TextView) findViewById(R.id.actualPerformanceMode);
        mPerformanceSpinner = (Spinner) findViewById(R.id.spinnerPerformanceMode);
        mPerformanceSpinner.setSelection(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY
                - StreamConfiguration.PERFORMANCE_MODE_NONE);

        mInputPresetTableRow = (TableRow) findViewById(R.id.rowInputPreset);
        mActualInputPresetView = (TextView) findViewById(R.id.actualInputPreset);
        mInputPresetSpinner = (Spinner) findViewById(R.id.spinnerInputPreset);
        mInputPresetSpinner.setSelection(2); // TODO need better way to select voice recording default

        mUsageTableRow = (TableRow) findViewById(R.id.rowUsage);
        mActualUsageView = (TextView) findViewById(R.id.actualUsage);
        mUsageSpinner = (Spinner) findViewById(R.id.spinnerUsage);
        mUsageSpinner.setSelection(0); // TODO need better way to select Media default

        mStreamInfoView = (TextView) findViewById(R.id.streamInfo);

        mStreamStatusView = (TextView) findViewById(R.id.statusView);

        mDeviceSpinner = (AudioDeviceSpinner) findViewById(R.id.devices_spinner);

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
        // Don't show Usage for input streams.
        mUsageTableRow.setVisibility(output ? View.VISIBLE : View.GONE);
    }

    public void applyToModel(StreamConfiguration config) {
        // Menu position matches actual enum value for these properties.
        config.setNativeApi(mNativeApiSpinner.getSelectedItemPosition());
        config.setChannelCount(mChannelCountSpinner.getSelectedItemPosition());
        config.setFormat(mFormatSpinner.getSelectedItemPosition());
        config.setRateConversionQuality(mRateConversionQualitySpinner.getSelectedItemPosition());

        int id =  ((AudioDeviceListEntry) mDeviceSpinner.getSelectedItem()).getId();
        config.setDeviceId(id);

        String text = mSampleRateSpinner.getSelectedItem().toString();
        int sampleRate = Integer.parseInt(text);
        config.setSampleRate(sampleRate);

        text = mInputPresetSpinner.getSelectedItem().toString();
        int inputPreset = StreamConfiguration.convertTextToInputPreset(text);
        config.setInputPreset(inputPreset);

        text = mUsageSpinner.getSelectedItem().toString();
        int usage = StreamConfiguration.convertTextToUsage(text);
        config.setUsage(usage);

        config.setMMap(mRequestedMMapView.isChecked());
        config.setChannelConversionAllowed(mChannelConversionBox.isChecked());
        config.setFormatConversionAllowed(mFormatConversionBox.isChecked());
        config.setSharingMode(mRequestedExclusiveView.isChecked()
                ? StreamConfiguration.SHARING_MODE_EXCLUSIVE
                : StreamConfiguration.SHARING_MODE_SHARED);
        config.setSessionId(mRequestAudioEffect.isChecked()
                ? StreamConfiguration.SESSION_ID_ALLOCATE
                : StreamConfiguration.SESSION_ID_NONE);

        config.setPerformanceMode(mPerformanceSpinner.getSelectedItemPosition()
                + StreamConfiguration.PERFORMANCE_MODE_NONE);
    }

    public void setChildrenEnabled(boolean enabled) {
        mNativeApiSpinner.setEnabled(enabled);
        mRequestedMMapView.setEnabled(enabled);
        mPerformanceSpinner.setEnabled(enabled);
        mRequestedExclusiveView.setEnabled(enabled);
        mChannelConversionBox.setEnabled(enabled);
        mFormatConversionBox.setEnabled(enabled);
        mChannelCountSpinner.setEnabled(enabled);
        mInputPresetSpinner.setEnabled(enabled);
        mUsageSpinner.setEnabled(enabled);
        mFormatSpinner.setEnabled(enabled);
        mSampleRateSpinner.setEnabled(enabled);
        mRateConversionQualitySpinner.setEnabled(enabled);
        mDeviceSpinner.setEnabled(enabled);
        mRequestAudioEffect.setEnabled(enabled);
    }

    // This must be called on the UI thread.
    void updateDisplay(StreamConfiguration actualConfiguration) {
        int value;

        value = actualConfiguration.getNativeApi();
        mActualNativeApiView.setText(StreamConfiguration.convertNativeApiToText(value));

        mActualMMapView.setText(yesOrNo(actualConfiguration.isMMap()));
        int sharingMode = actualConfiguration.getSharingMode();
        boolean isExclusive = (sharingMode == StreamConfiguration.SHARING_MODE_EXCLUSIVE);
        mActualExclusiveView.setText(yesOrNo(isExclusive));

        value = actualConfiguration.getPerformanceMode();
        mActualPerformanceView.setText(StreamConfiguration.convertPerformanceModeToText(value));
        mActualPerformanceView.requestLayout();

        value = actualConfiguration.getFormat();
        mActualFormatView.setText(StreamConfiguration.convertFormatToText(value));
        mActualFormatView.requestLayout();

        value = actualConfiguration.getInputPreset();
        mActualInputPresetView.setText(StreamConfiguration.convertInputPresetToText(value));
        mActualInputPresetView.requestLayout();

        value = actualConfiguration.getUsage();
        mActualUsageView.setText(StreamConfiguration.convertUsageToText(value));
        mActualUsageView.requestLayout();

        mActualChannelCountView.setText(actualConfiguration.getChannelCount() + "");
        mActualSampleRateView.setText(actualConfiguration.getSampleRate() + "");
        mActualSessionIdView.setText("S#: " + actualConfiguration.getSessionId());

        boolean isMMap = actualConfiguration.isMMap();
        mStreamInfoView.setText("burst = " + actualConfiguration.getFramesPerBurst()
                + ", capacity = " + actualConfiguration.getBufferCapacityInFrames()
                + ", devID = " + actualConfiguration.getDeviceId()
                + ", " + (actualConfiguration.isMMap() ? "MMAP" : "Legacy")
                + (isMMap ? ", " + StreamConfiguration.convertSharingModeToText(sharingMode) : "")
        );

        mHideableView.requestLayout();
    }

    // This must be called on the UI thread.
    public void setStatusText(String msg) {
        mStreamStatusView.setText(msg);
    }

    public void setExclusiveMode(boolean b) {
        mRequestedExclusiveView.setChecked(b);
    }

    public void setFormat(int format) {
        mFormatSpinner.setSelection(format); // position matches format
    }

    public void setFormatConversionAllowed(boolean allowed) {
        mFormatConversionBox.setChecked(allowed);
    }

}
