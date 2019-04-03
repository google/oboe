package com.google.sample.oboe.manualtest;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;


import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.LinearLayout;

public class BufferSizeView extends LinearLayout {

    AudioOutputTester mAudioOutTester;

    protected static final int FADER_THRESHOLD_MAX = 1000;
    protected TextView mTextThreshold;
    protected SeekBar mFaderThreshold;
    protected ExponentialTaper mTaperThreshold;

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
        mTaperThreshold = new ExponentialTaper(FADER_THRESHOLD_MAX, 0.0, 1.0, 10.0);
        mFaderThreshold.setProgress(FADER_THRESHOLD_MAX / 2);
    }

    private void setBufferSizeByPosition(int progress) {
        StringBuffer message = new StringBuffer();
        double normalizedThreshold = mTaperThreshold.linearToExponential(progress);
        if (normalizedThreshold < 0.0) normalizedThreshold = 0.0;
        else if (normalizedThreshold > 1.0) normalizedThreshold = 1.0;
        message.append("bufferSize = ");
        if (mAudioOutTester != null) {
            mAudioOutTester.setNormalizedThreshold(normalizedThreshold);
            int percent = (int) (normalizedThreshold * 100);
            message.append(percent + "%");
            int bufferSize = mAudioOutTester.getCurrentAudioStream().getBufferSizeInFrames();
            int bufferCapacity = mAudioOutTester.getCurrentAudioStream().getBufferCapacityInFrames();
            if (bufferSize >= 0) {
                message.append(" = " + bufferSize + " / " + bufferCapacity);
            }
        } else {
            mTextThreshold.setText("bufferSize = null!!! " + progress);
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
