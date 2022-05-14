/*
 * Copyright 2022 The Android Open Source Project
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

package com.mobileer.oboetester;

import android.content.Intent;
import android.os.Bundle;

public class IntentBasedTestSupport {

    public static final String KEY_IN_SHARING = "in_sharing";
    public static final String KEY_OUT_SHARING = "out_sharing";
    public static final String VALUE_SHARING_EXCLUSIVE = "exclusive";
    public static final String VALUE_SHARING_SHARED = "shared";
    public static final String KEY_IN_PERF = "in_perf";
    public static final String KEY_OUT_PERF = "out_perf";
    public static final String VALUE_PERF_LOW_LATENCY = "lowlat";
    public static final String VALUE_PERF_POWERSAVE = "powersave";
    public static final String VALUE_PERF_NONE = "none";
    public static final String KEY_IN_CHANNELS = "in_channels";
    public static final String KEY_OUT_CHANNELS = "out_channels";
    public static final int VALUE_DEFAULT_CHANNELS = 2;
    public static final String KEY_IN_USE_MMAP = "in_use_mmap";
    public static final String KEY_OUT_USE_MMAP = "out_use_mmap";
    public static final String KEY_IN_PRESET = "in_preset";
    public static final String KEY_SAMPLE_RATE = "sample_rate";
    public static final int VALUE_DEFAULT_SAMPLE_RATE = 48000;
    public static final String VALUE_UNSPECIFIED = "unspecified";
    public static final String KEY_IN_API = "in_api";
    public static final String KEY_OUT_API = "out_api";
    public static final String VALUE_API_AAUDIO = "aaudio";
    public static final String VALUE_API_OPENSLES = "opensles";
    protected static final String KEY_FILE_NAME = "file";
    protected static final String KEY_BUFFER_BURSTS = "buffer_bursts";
    protected static final String KEY_BACKGROUND = "background";
    protected static final String KEY_VOLUME = "volume";

    public static int getApiFromText(String text) {
        if (VALUE_API_AAUDIO.equals(text)) {
            return StreamConfiguration.NATIVE_API_AAUDIO;
        } else if (VALUE_API_OPENSLES.equals(text)) {
            return StreamConfiguration.NATIVE_API_OPENSLES;
        } else {
            return StreamConfiguration.NATIVE_API_UNSPECIFIED;
        }
    }

    public static int getPerfFromText(String text) {
        if (VALUE_PERF_NONE.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_NONE;
        } else if (VALUE_PERF_POWERSAVE.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_POWER_SAVING;
        } else {
            return StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY;
        }
    }

    public static int getSharingFromText(String text) {
        if (VALUE_SHARING_SHARED.equals(text)) {
            return StreamConfiguration.SHARING_MODE_SHARED;
        } else {
            return StreamConfiguration.SHARING_MODE_EXCLUSIVE;
        }
    }

    public static void configureStreamsFromBundle(Bundle bundle,
                                                  StreamConfiguration requestedInConfig,
                                                  StreamConfiguration requestedOutConfig) {
        configureInputStreamFromBundle(bundle, requestedInConfig);
        configureOutputStreamFromBundle(bundle, requestedOutConfig);
    }

    public static float getNormalizedVolumeFromBundle(Bundle bundle) {
        return bundle.getFloat(KEY_VOLUME, -1.0f);
    }

    public static void configureOutputStreamFromBundle(Bundle bundle,
                                                        StreamConfiguration requestedOutConfig) {
        int audioApi;
        String text;

        requestedOutConfig.reset();

        int sampleRate = bundle.getInt(KEY_SAMPLE_RATE, VALUE_DEFAULT_SAMPLE_RATE);
        requestedOutConfig.setSampleRate(sampleRate);

        text = bundle.getString(KEY_OUT_API, VALUE_UNSPECIFIED);
        audioApi = getApiFromText(text);
        requestedOutConfig.setNativeApi(audioApi);

        int outChannels = bundle.getInt(KEY_OUT_CHANNELS, VALUE_DEFAULT_CHANNELS);
        requestedOutConfig.setChannelCount(outChannels);

        boolean outMMAP = bundle.getBoolean(KEY_OUT_USE_MMAP, true);
        requestedOutConfig.setMMap(outMMAP);

        text = bundle.getString(KEY_OUT_PERF, VALUE_PERF_LOW_LATENCY);
        int perfMode = getPerfFromText(text);
        requestedOutConfig.setPerformanceMode(perfMode);

        text = bundle.getString(KEY_OUT_SHARING, VALUE_SHARING_EXCLUSIVE);
        int sharingMode = getSharingFromText(text);
        requestedOutConfig.setSharingMode(sharingMode);

    }

    public static void configureInputStreamFromBundle(Bundle bundle,
                                                       StreamConfiguration requestedInConfig) {
        int audioApi;
        String text;

        requestedInConfig.reset();

        int sampleRate = bundle.getInt(KEY_SAMPLE_RATE, VALUE_DEFAULT_SAMPLE_RATE);
        requestedInConfig.setSampleRate(sampleRate);

        text = bundle.getString(KEY_IN_API, VALUE_UNSPECIFIED);
        audioApi = getApiFromText(text);
        requestedInConfig.setNativeApi(audioApi);

        int inChannels = bundle.getInt(KEY_IN_CHANNELS, VALUE_DEFAULT_CHANNELS);
        requestedInConfig.setChannelCount(inChannels);

        boolean inMMAP = bundle.getBoolean(KEY_IN_USE_MMAP, true);
        requestedInConfig.setMMap(inMMAP);

        text = bundle.getString(KEY_IN_PERF, VALUE_PERF_LOW_LATENCY);
        int perfMode = getPerfFromText(text);
        requestedInConfig.setPerformanceMode(perfMode);

        text = bundle.getString(KEY_IN_SHARING, VALUE_SHARING_EXCLUSIVE);
        int sharingMode = getSharingFromText(text);
        requestedInConfig.setSharingMode(sharingMode);

        String defaultText = StreamConfiguration.convertInputPresetToText(
                StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION);
        text = bundle.getString(KEY_IN_PRESET, defaultText);
        int inputPreset = StreamConfiguration.convertTextToInputPreset(text);
        requestedInConfig.setInputPreset(inputPreset);
    }

}
