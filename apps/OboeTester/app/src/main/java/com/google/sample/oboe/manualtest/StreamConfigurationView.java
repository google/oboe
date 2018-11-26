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
import android.widget.TextView;
import android.widget.LinearLayout;

import com.google.sample.audio_device.AudioDeviceListEntry;
import com.google.sample.audio_device.AudioDeviceSpinner;
import com.google.sample.oboe.manualtest.R;

/**
 * View for Editing a requested StreamConfiguration
 * and displaying the actual StreamConfiguration.
 */

public class StreamConfigurationView extends LinearLayout {

    private StreamConfiguration  mRequestedConfiguration = new StreamConfiguration();
    private StreamConfiguration  mActualConfiguration = new StreamConfiguration();

    protected Spinner mNativeApiSpinner;
    private TextView mActualNativeApiView;

    private TextView mActualExclusiveView;
    private TextView mActualPerformanceView;
    private Spinner  mPerformanceSpinner;
    private CheckBox mRequestedExclusiveView;
    private TextView mStreamInfoView;
    private Spinner  mChannelCountSpinner;
    private TextView mActualChannelCountView;
    private TextView mActualFormatView;
    private Spinner  mFormatSpinner;
    private Spinner  mSampleRateSpinner;
    private TextView mActualSampleRateView;
    private TableLayout mOptionTable;

    private AudioDeviceSpinner mDeviceSpinner;
    private TextView mActualSessionIdView;
    private CheckBox mRequestAudioEffect;


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

        mOptionTable = (TableLayout) findViewById(R.id.optionTable);

        mNativeApiSpinner = (Spinner) findViewById(R.id.spinnerNativeApi);
        mNativeApiSpinner.setOnItemSelectedListener(new NativeApiSpinnerListener());
        mNativeApiSpinner.setSelection(StreamConfiguration.NATIVE_API_UNSPECIFIED);

        mActualNativeApiView = (TextView) findViewById(R.id.actualNativeApi);

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

        mActualPerformanceView = (TextView) findViewById(R.id.actualPerformanceMode);
        mPerformanceSpinner = (Spinner) findViewById(R.id.spinnerPerformanceMode);
        mPerformanceSpinner.setOnItemSelectedListener(new PerformanceModeSpinnerListener());
        mPerformanceSpinner.setSelection(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY
                - StreamConfiguration.PERFORMANCE_MODE_NONE);

        mStreamInfoView = (TextView) findViewById(R.id.streamInfo);

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
    }

    public void setOutput(boolean output) {
        if (output) {
            mDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_OUTPUTS);
        } else {
            mDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_INPUTS);
        }
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
            mRequestedConfiguration.setPerformanceMode(StreamConfiguration.UNSPECIFIED);
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
            mRequestedConfiguration.setPerformanceMode(StreamConfiguration.UNSPECIFIED);
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

    void updateDisplay() {
        int value;

        value = mActualConfiguration.getNativeApi();
        mActualNativeApiView.setText(StreamConfiguration.convertNativeApiToText(value));

        value = mActualConfiguration.getSharingMode();
        mActualExclusiveView.setText(StreamConfiguration.convertSharingModeToText(value));

        value = mActualConfiguration.getPerformanceMode();
        mActualPerformanceView.setText(StreamConfiguration.convertPerformanceModeToText(value));
        mActualPerformanceView.requestLayout();

        value = mActualConfiguration.getFormat();
        mActualFormatView.setText(StreamConfiguration.convertFormatToText(value));
        mActualFormatView.requestLayout();

        mActualChannelCountView.setText(mActualConfiguration.getChannelCount() + "");
        mActualSampleRateView.setText(mActualConfiguration.getSampleRate() + "");
        mActualSessionIdView.setText("S#: " + mActualConfiguration.getSessionId());

        mStreamInfoView.setText("burst = " + mActualConfiguration.getFramesPerBurst()
                + ", capacity = " + mActualConfiguration.getBufferCapacityInFrames()
                + ", devID = " + mActualConfiguration.getDeviceId()
                + ", " + (mActualConfiguration.isMMap() ? "MMAP" : "Legacy")
        );

        mOptionTable.requestLayout();
    }

    public StreamConfiguration getRequestedConfiguration() {
        return mRequestedConfiguration;
    }

    public StreamConfiguration getActualConfiguration() {
        return mActualConfiguration;
    }
}
