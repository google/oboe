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

import java.io.IOException;


abstract class TestOutputActivityBase extends TestAudioActivity {
    AudioOutputTester mAudioOutTester;

    private BufferSizeView mBufferSizeView;
    private WorkloadView mWorkloadView;

    @Override boolean isOutput() { return true; }

    @Override
    protected void resetConfiguration() {
        super.resetConfiguration();
        mAudioOutTester.reset();
    }

    protected void findAudioCommon() {
        super.findAudioCommon();
        mBufferSizeView = (BufferSizeView) findViewById(R.id.buffer_size_view);
        mWorkloadView = (WorkloadView) findViewById(R.id.workload_view);
    }

    @Override
    public AudioOutputTester addAudioOutputTester() {
        AudioOutputTester audioOutTester = super.addAudioOutputTester();
        mBufferSizeView.setAudioOutTester(audioOutTester);
        mWorkloadView.setAudioStreamTester(audioOutTester);
        return audioOutTester;
    }


    @Override
    public void openAudio() throws IOException {
        super.openAudio();
        if (mBufferSizeView != null) {
            mBufferSizeView.updateBufferSize();
        }
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