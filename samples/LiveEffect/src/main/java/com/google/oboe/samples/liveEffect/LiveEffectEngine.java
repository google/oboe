package com.google.oboe.samples.liveEffect;
/*
 * Copyright 2018 The Android Open Source Project
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

import android.content.Context;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.BassBoost;
import android.media.audiofx.LoudnessEnhancer;
import android.media.audiofx.PresetReverb;
import android.media.AudioManager;
import android.media.metrics.PlaybackSession;
import android.os.Build;
import android.util.Log;

public enum LiveEffectEngine {
    INSTANCE;

    // Load native library
    static {
        System.loadLibrary("liveEffect");
    }

    private static final String TAG = LiveEffectEngine.class.getName();

    // Native methods
    static native boolean create();
    static native boolean isAAudioRecommended();
    static native boolean setAPI(int apiType);
    static native boolean openStreams();
    static native int getRecordSessionId();
    static native int getPlaybackSessionId();
    static native boolean startDuplexEffects();
    static native void closeStreams();
    static native boolean hasEffectStarted();
    static native void setRecordingDeviceId(int deviceId);
    static native void setPlaybackDeviceId(int deviceId);
    static native void delete();
    static native void native_setDefaultStreamValues(int defaultSampleRate, int defaultFramesPerBurst);

    static boolean startEffects(boolean useAcousticEchoCanceler, boolean usePresetReverb) {
        boolean success = false;
        if (!hasEffectStarted()) {
            success = openStreams();
            if (success) {
                if (startDuplexEffects()) {
                    if (useAcousticEchoCanceler) {
                        int recordSessionId = getRecordSessionId();
                        Log.d(TAG, "Record Session Id: " + recordSessionId);
                        AcousticEchoCanceler effect = AcousticEchoCanceler.create(recordSessionId);
                        if (effect == null) {
                            Log.e(TAG, "Could not create AcousticEchoCanceler");
                        }
                    }
                    if (usePresetReverb) {
                        int playbackSessionId = getPlaybackSessionId();
                        Log.d(TAG, "Playback Session Id: " + playbackSessionId);
                        PresetReverb effect = new PresetReverb(0, playbackSessionId);
                        if (effect == null) {
                            Log.e(TAG, "Could not create LoudnessEnhancer");
                        }
                    }
                }
            }
        }
        return success;
    }

    static boolean stopEffects() {
        boolean success = false;
        if (hasEffectStarted()) {
            closeStreams();
            success = true;
        }
        return success;
    }

    static void setDefaultStreamValues(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1){
            AudioManager myAudioMgr = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            String sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            int defaultSampleRate = Integer.parseInt(sampleRateStr);
            String framesPerBurstStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            int defaultFramesPerBurst = Integer.parseInt(framesPerBurstStr);

            native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst);
        }
    }
}
