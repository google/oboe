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

import android.util.Log;

public class AudioOutputTester extends AudioStreamTester {

    protected OboeAudioOutputStream mOboeAudioOutputStream;

    private static AudioOutputTester mInstance;

    public static synchronized AudioOutputTester getInstance() {
        if (mInstance == null) {
            mInstance = new AudioOutputTester();
        }
        return mInstance;
    }

    private AudioOutputTester() {
        super();
        Log.i(TapToToneActivity.TAG, "create OboeAudioOutputStream ---------");
        mOboeAudioOutputStream = new OboeAudioOutputStream();
        mCurrentAudioStream = mOboeAudioOutputStream;
        setToneType(OboeAudioOutputStream.TONE_TYPE_SINE_STEADY);
        setEnabled(false);
    }

    public void setToneType(int index) {
        mOboeAudioOutputStream.setToneType(index);
    }

    public void setEnabled(boolean flag) {
        mOboeAudioOutputStream.setToneEnabled(flag);
    }

    public void setNormalizedThreshold(double threshold) {
        if (mCurrentAudioStream.isThresholdSupported()) {
            int frames = (int) (threshold * mCurrentAudioStream.getBufferCapacityInFrames());
            Log.i(TapToToneActivity.TAG, mCurrentAudioStream.getClass().getSimpleName()
                    + ".setBufferSizeInFrames(" + frames + ")");
            mCurrentAudioStream.setBufferSizeInFrames(frames);
        }
    }

    public void setAmplitude(double amplitude) {
        mCurrentAudioStream.setAmplitude(amplitude);
    }

    public void setChannelEnabled(int channelIndex, boolean enabled)  {
        mOboeAudioOutputStream.setChannelEnabled(channelIndex, enabled);
    }
}
