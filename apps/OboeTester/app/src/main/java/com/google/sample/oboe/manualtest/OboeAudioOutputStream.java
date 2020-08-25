/*
 * Copyright 2015 The Android Open Source Project
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

/**
 * Native synthesizer and audio output.
 */
public class OboeAudioOutputStream extends OboeAudioStream {

    // WARNING - must match order in strings.xml
    public static final int TONE_TYPE_SAW_PING = 0;
    public static final int TONE_TYPE_SINE = 1;
    public static final int TONE_TYPE_IMPULSE = 2;
    public static final int TONE_TYPE_SAWTOOTH = 3;

    @Override
    public boolean isInput() {
        return false;
    }

    public native void setToneEnabled(boolean enabled);

    public native void setToneType(int index);

    public native void setChannelEnabled(int channelIndex, boolean enabled);

    public native void setSignalType(int type);
}
