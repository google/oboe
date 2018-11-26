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

/**
 * Container for the properties of a Stream.
 *
 * This can be used to build a stream, or as a base class for a Stream,
 * or as a way to report the properties of a Stream.
 */

public class StreamConfiguration {
    public static final int UNSPECIFIED = 0;

    // These must match order in Spinner and in native code.
    public static final int NATIVE_API_UNSPECIFIED = 0;
    public static final int NATIVE_API_OPENSLES = 1;
    public static final int NATIVE_API_AAUDIO = 2;

    public static final int SHARING_MODE_EXCLUSIVE = 0; // must match AAUDIO
    public static final int SHARING_MODE_SHARED = 1; // must match AAUDIO

    public static final int AUDIO_FORMAT_PCM_16 = 1; // must match AAUDIO
    public static final int AUDIO_FORMAT_PCM_FLOAT = 2; // must match AAUDIO

    public static final int DIRECTION_OUTPUT = 0; // must match AAUDIO
    public static final int DIRECTION_INPUT = 1; // must match AAUDIO

    public static final int SESSION_ID_NONE = -1; // must match AAUDIO
    public static final int SESSION_ID_ALLOCATE = 0; // must match AAUDIO

    public static final int PERFORMANCE_MODE_NONE = 10; // must match AAUDIO
    public static final int PERFORMANCE_MODE_POWER_SAVING = 11; // must match AAUDIO
    public static final int PERFORMANCE_MODE_LOW_LATENCY = 12; // must match AAUDIO


    private int mNativeApi;
    private int mBufferCapacityInFrames = UNSPECIFIED;
    private int mChannelCount = UNSPECIFIED;
    private int mDeviceId = UNSPECIFIED;
    private int mSessionId = -1;
    private int mDirection = DIRECTION_OUTPUT;
    private int mFormat = AUDIO_FORMAT_PCM_FLOAT;
    private int mSampleRate = UNSPECIFIED;
    private int mSharingMode = SHARING_MODE_SHARED;
    private int mPerformanceMode = PERFORMANCE_MODE_LOW_LATENCY;
    private int mFramesPerBurst = 29; // TODO review
    private boolean mMMap = false;

    public void setReasonableDefaults() {
        mChannelCount = 2;
        mSampleRate = 48000;
    }

    public int getFramesPerBurst() {
        return mFramesPerBurst;
    }

    public void setFramesPerBurst(int framesPerBurst) {
        this.mFramesPerBurst = framesPerBurst;
    }

    public int getBufferCapacityInFrames() {
        return mBufferCapacityInFrames;
    }

    public void setBufferCapacityInFrames(int bufferCapacityInFrames) {
        this.mBufferCapacityInFrames = bufferCapacityInFrames;
    }

    public int getFormat() {
        return mFormat;
    }

    public void setFormat(int format) {
        this.mFormat = format;
    }

    public int getDirection() {
        return mDirection;
    }

    public void setDirection(int direction) {
        this.mDirection = direction;
    }

    public int getPerformanceMode() {
        return mPerformanceMode;
    }

    public void setPerformanceMode(int performanceMode) {
        this.mPerformanceMode = performanceMode;
    }

    static String convertPerformanceModeToText(int performanceMode) {
        switch(performanceMode) {
            case PERFORMANCE_MODE_NONE:
                return "NONE";
            case PERFORMANCE_MODE_POWER_SAVING:
                return "POWER_SAVING";
            case PERFORMANCE_MODE_LOW_LATENCY:
                return "LOW_LATENCY";
            default:
                return "INVALID";
        }
    }

    public int getSharingMode() {
        return mSharingMode;
    }

    public void setSharingMode(int sharingMode) {
        this.mSharingMode = sharingMode;
    }

    static String convertSharingModeToText(int sharingMode) {
        switch(sharingMode) {
            case SHARING_MODE_SHARED:
                return "SHARED";
            case SHARING_MODE_EXCLUSIVE:
                return "EXCLUSIVE";
            default:
                return "INVALID";
        }
    }

    public static String convertFormatToText(int format) {
        switch(format) {
            case UNSPECIFIED:
                return "Unspecified";
            case AUDIO_FORMAT_PCM_16:
                return "I16";
            case AUDIO_FORMAT_PCM_FLOAT:
                return "Float";
            default:
                return "Invalid";
        }
    }

    public static String convertNativeApiToText(int api) {
        switch(api) {
            case NATIVE_API_UNSPECIFIED:
                return "Unspec";
            case NATIVE_API_AAUDIO:
                return "AAudio";
            case NATIVE_API_OPENSLES:
                return "OpenSL";
            default:
                return "Invalid";
        }
    }

    public int getChannelCount() {
        return mChannelCount;
    }

    public void setChannelCount(int channelCount) {
        this.mChannelCount = channelCount;
    }

    public int getSampleRate() {
        return mSampleRate;
    }

    public void setSampleRate(int sampleRate) {
        this.mSampleRate = sampleRate;
    }

    public int getDeviceId() {
        return mDeviceId;
    }

    public void setDeviceId(int deviceId) {
        this.mDeviceId = deviceId;
    }

    public int getSessionId() {
        return mSessionId;
    }

    public void setSessionId(int sessionId) {
        mSessionId = sessionId;
    }

    public boolean isMMap() {
        return mMMap;
    }
    public void setMMap(boolean b) {
        mMMap = b;
    }

    public int getNativeApi() {
        return mNativeApi;
    }

    public void setNativeApi(int nativeApi) {
        mNativeApi = nativeApi;
    }

}
