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


import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.LinearLayout;

public class BufferSizeView extends LinearLayout {

    AudioOutputTester mAudioOutTester;

    protected static final int FADER_THRESHOLD_MAX = 1000; // must match layout
    protected TextView mTextThreshold;
    protected SeekBar mFaderThreshold;
    protected ExponentialTaper mTaperThreshold;
    private int mCachedCapacity;

    private SeekBar.OnSeekBarChangeListener mThresholdListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            setBufferSizeByPosition(progress);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };

    public BufferSizeView(Context context) {
        super(context);
        initializeViews(context);
    }

    public BufferSizeView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public BufferSizeView(Context context,
                                   AttributeSet attrs,
                                   int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    public AudioOutputTester getAudioOutTester() {
        return mAudioOutTester;
    }

    public void setAudioOutTester(AudioOutputTester audioOutTester) {
        mAudioOutTester = audioOutTester;
    }

    void setFaderNormalizedProgress(double fraction) {
        mFaderThreshold.setProgress((int)(fraction * FADER_THRESHOLD_MAX));
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
        inflater.inflate(R.layout.buffer_size_view, this);

        mTextThreshold = (TextView) findViewById(R.id.textThreshold);
        mFaderThreshold = (SeekBar) findViewById(R.id.faderThreshold);
        mFaderThreshold.setOnSeekBarChangeListener(mThresholdListener);
        mTaperThreshold = new ExponentialTaper(0.0, 1.0, 10.0);
        mFaderThreshold.setProgress(0);
    }

    private void setBufferSizeByPosition(int progress) {
        StringBuffer message = new StringBuffer();
        double normalizedThreshold = mTaperThreshold.linearToExponential(
                ((double)progress)/FADER_THRESHOLD_MAX);
        if (normalizedThreshold < 0.0) normalizedThreshold = 0.0;
        else if (normalizedThreshold > 1.0) normalizedThreshold = 1.0;
        int  percent = (int) (normalizedThreshold * 100);
        message.append("bufferSize = " + percent + "%");

        OboeAudioStream stream = null;
        int sizeFrames = 0;
        if (getAudioOutTester()  != null) {
            stream = (OboeAudioStream) getAudioOutTester().getCurrentAudioStream();
            if (stream != null) {
                int capacity = stream.getBufferCapacityInFrames();
                if (capacity > 0) mCachedCapacity = capacity;
            }
        }
        if (mCachedCapacity > 0) {
            sizeFrames = (int) (normalizedThreshold * mCachedCapacity);
            message.append(" = " + sizeFrames);
            if (stream != null) {
                stream.setBufferSizeInFrames(sizeFrames);
            }
            int bufferSize = getAudioOutTester().getCurrentAudioStream().getBufferSizeInFrames();
            if (bufferSize >= 0) {
                message.append(" / " + bufferSize);
            }
            message.append(" / " + mCachedCapacity);
        }
        mTextThreshold.setText(message.toString());
    }

    public void updateBufferSize() {
        int progress = mFaderThreshold.getProgress();
        setBufferSizeByPosition(progress);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mFaderThreshold.setEnabled(enabled);
    }
}
