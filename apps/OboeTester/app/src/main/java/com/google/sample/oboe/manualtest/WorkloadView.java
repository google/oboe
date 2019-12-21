/*
 * Copyright 2019 The Android Open Source Project
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
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

public class WorkloadView extends LinearLayout {

    private AudioStreamTester mAudioStreamTester;

    protected static final int FADER_PROGRESS_MAX = 1000; // must match layout
    protected TextView mTextView;
    protected SeekBar mSeekBar;
    protected ExponentialTaper mExponentialTaper;

    private SeekBar.OnSeekBarChangeListener mChangeListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            setValueByPosition(progress);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };

    public WorkloadView(Context context) {
        super(context);
        initializeViews(context);
    }

    public WorkloadView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public WorkloadView(Context context,
                        AttributeSet attrs,
                        int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    public AudioStreamTester getAudioStreamTester() {
        return mAudioStreamTester;
    }

    public void setAudioStreamTester(AudioStreamTester audioStreamTester) {
        mAudioStreamTester = audioStreamTester;
    }

    void setFaderNormalizedProgress(double fraction) {
        mSeekBar.setProgress((int)(fraction * FADER_PROGRESS_MAX));
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
        inflater.inflate(R.layout.workload_view, this);

        mTextView = (TextView) findViewById(R.id.textWorkload);
        mSeekBar = (SeekBar) findViewById(R.id.faderWorkload);
        mSeekBar.setOnSeekBarChangeListener(mChangeListener);
        mExponentialTaper = new ExponentialTaper(0.0, 100.0, 10.0);
        //mSeekBar.setProgress(0);
    }

    private void setValueByPosition(int progress) {
        double workload = mExponentialTaper.linearToExponential(
                ((double)progress) / FADER_PROGRESS_MAX);
        mAudioStreamTester.setWorkload(workload);
        mTextView.setText("Workload = " + String.format("%6.2f", workload));
    }

    public void updateBufferSize() {
        int progress = mSeekBar.getProgress();
        setValueByPosition(progress);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mSeekBar.setEnabled(enabled);
    }
}
