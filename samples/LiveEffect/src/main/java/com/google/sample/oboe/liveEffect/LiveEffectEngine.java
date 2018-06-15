package com.google.sample.oboe.liveEffect;
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
import android.content.res.AssetManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.util.Log;

public enum LiveEffectEngine {

    INSTANCE;

    // Load native library
    static {
        System.loadLibrary("liveEffect");
    }

    // Native methods
    static native boolean create();
    static native boolean isAAudioSupported();
    static native boolean setAPI(int apiType);
    static native void setEffectOn(boolean isEffectOn);
    static native void setRecordingDeviceId(int deviceId);
    static native void setPlaybackDeviceId(int deviceId);
    static native void delete();
}
