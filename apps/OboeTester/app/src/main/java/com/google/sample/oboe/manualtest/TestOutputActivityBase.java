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

import android.media.audiofx.Equalizer;
import android.media.audiofx.PresetReverb;
import android.util.Log;
import android.widget.SeekBar;
import android.widget.TextView;

import com.google.sample.oboe.manualtest.R;


class TestOutputActivityBase extends TestAudioActivity {
    AudioOutputTester mAudioOutTester;

    protected TextView mTextThreshold;
    protected SeekBar mFaderThreshold;
    protected ExponentialTaper mTaperThreshold;
    protected TextView mTextAmplitude;
    protected SeekBar mFaderAmplitude;
    protected ExponentialTaper mTaperAmplitude;

    @Override boolean isOutput() { return true; }

    protected void updateEnabledWidgets() {
        super.updateEnabledWidgets();
        boolean thresholdSupported = (mAudioStreamTester == null) ? false :
                mAudioStreamTester.getCurrentAudioStream().isThresholdSupported();
        mFaderThreshold.setEnabled(getState() == STATE_STARTED && thresholdSupported);
    }

    private SeekBar.OnSeekBarChangeListener mAmplitudeListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            double amplitude = mTaperAmplitude.linearToExponential(progress);
            mAudioOutTester.setAmplitude(amplitude);
            mTextAmplitude.setText("Amplitude = " + amplitude);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };

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

    private void setBufferSizeByPosition(int progress) {
        double normalizedThreshold = mTaperThreshold.linearToExponential(progress);
        if (normalizedThreshold < 0.0) normalizedThreshold = 0.0;
        else if (normalizedThreshold > 1.0) normalizedThreshold = 1.0;
        if (mAudioStreamTester != null) {
            mAudioOutTester.setNormalizedThreshold(normalizedThreshold);
            int percent = (int) (normalizedThreshold * 100);
            int bufferSize = mAudioStreamTester.getCurrentAudioStream().getBufferSizeInFrames();
            int bufferCapacity = mAudioStreamTester.getCurrentAudioStream().getBufferCapacityInFrames();
            mTextThreshold.setText("bufferSize = " + percent + "% = "
                    + bufferSize + " / " + bufferCapacity);
        }
    }

    protected void findAudioCommon() {
        super.findAudioCommon();

        mTextThreshold = (TextView) findViewById(R.id.textThreshold);
        mFaderThreshold = (SeekBar) findViewById(R.id.faderThreshold);
        mFaderThreshold.setOnSeekBarChangeListener(mThresholdListener);
        mTaperThreshold = new ExponentialTaper(FADER_THRESHOLD_MAX, 0.01, 1.0);
        mFaderThreshold.setProgress(FADER_THRESHOLD_MAX / 2);

        mTextAmplitude = (TextView) findViewById(R.id.textAmplitude);
        mFaderAmplitude = (SeekBar) findViewById(R.id.faderAmplitude);
        mFaderAmplitude.setOnSeekBarChangeListener(mAmplitudeListener);
        mTaperAmplitude = new ExponentialTaper(FADER_THRESHOLD_MAX, 0.01, 4.0);
    }

    public void pauseAudio() {
        super.pauseAudio();
    }

    public void stopAudio() {
        super.stopAudio();
    }

    // TODO Add editor
    public void setupEqualizer(int sessionId) {
        Equalizer equalizer = new Equalizer(0, sessionId);
        int numBands = equalizer.getNumberOfBands();
        Log.d(TAG, "numBands " + numBands);
        for (short band = 0; band < numBands; band++) {
            String msg = "band " + band
                    + ", center = " + equalizer.getCenterFreq(band)
                    + ", level = " + equalizer.getBandLevel(band);
            Log.d(TAG, msg);
            equalizer.setBandLevel(band, (short)40);
        }

        equalizer.setBandLevel((short) 1, (short) 300);
    }

    public void setupReverb(int sessionId) {
        PresetReverb effect = new PresetReverb(0, sessionId);
    }

    @Override
    public void setupEffects(int sessionId) {
        // setupEqualizer(sessionId);
        // setupReverb(sessionId);
    }
}