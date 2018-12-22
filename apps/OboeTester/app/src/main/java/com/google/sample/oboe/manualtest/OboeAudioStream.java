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

import java.io.IOException;

/**
 * Created by philburk on 12/10/17.
 */

abstract class OboeAudioStream extends AudioStreamBase {
    @Override
    public void start() throws IOException {
        int result = startNative();
        if (result < 0) {
            throw new IOException("Start failed! result = " + result);
        }
    }

    public native int startNative();

    @Override
    public void pause() throws IOException {
        int result = pauseNative();
        if (result < 0) {
            throw new IOException("Pause failed! result = " + result);
        }
    }

    public native int pauseNative();

    @Override
    public void stop() throws IOException {
        int result = stopNative();
        if (result < 0) {
            throw new IOException("Stop failed! result = " + result);
        }
    }

    public native int stopNative();


    @Override
    public void stopPlayback() throws IOException {
        int result = stopPlaybackNative();
        if (result < 0) {
            throw new IOException("Stop Playback failed! result = " + result);
        }
    }

    public native int stopPlaybackNative();

    @Override
    public void startPlayback() throws IOException {
        int result = startPlaybackNative();
        if (result < 0) {
            throw new IOException("Start Playback failed! result = " + result);
        }
    }

    public native int startPlaybackNative();

    // Write disabled because the synth is in native code.
    @Override
    public int write(float[] buffer, int offset, int length) {
        return 0;
    }

    @Override
    public void open(StreamConfiguration requestedConfiguration,
                     StreamConfiguration actualConfiguration, int bufferSizeInFrames) throws IOException {
        super.open(requestedConfiguration, actualConfiguration, bufferSizeInFrames);
        setNativeApi(requestedConfiguration.getNativeApi());
        int result = openNative(requestedConfiguration.getSampleRate(),
                requestedConfiguration.getChannelCount(),
                requestedConfiguration.getFormat(),
                requestedConfiguration.getSharingMode(),
                requestedConfiguration.getPerformanceMode(),
                requestedConfiguration.getDeviceId(),
                requestedConfiguration.getSessionId(),
                requestedConfiguration.getFramesPerBurst(),
                isInput());
        if (result < 0) {
            throw new IOException("Open failed! result = " + result);
        }
        actualConfiguration.setNativeApi(getNativeApi());
        actualConfiguration.setSampleRate(getSampleRate());
        actualConfiguration.setSharingMode(getSharingMode());
        actualConfiguration.setPerformanceMode(getPerformanceMode());
        actualConfiguration.setFramesPerBurst(getFramesPerBurst());
        actualConfiguration.setBufferCapacityInFrames(getBufferCapacityInFrames());
        actualConfiguration.setChannelCount(getChannelCount());
        actualConfiguration.setDeviceId(getDeviceId());
        actualConfiguration.setSessionId(getSessionId());
        actualConfiguration.setFormat(getFormat());
        actualConfiguration.setMMap(isMMap());
    }

    private native int openNative(
            int sampleRate,
            int channelCount,
            int sharingMode,
            int performanceMode,
            int deviceId,
            int sessionId,
            int framesPerRead,
            int perRead, boolean isInput);

    public native void close();

    @Override
    public native int getBufferCapacityInFrames();

    @Override
    public native int getBufferSizeInFrames();

    @Override
    public boolean isThresholdSupported() {
        return true;
    }

    @Override
    public native int setBufferSizeInFrames(int thresholdFrames);

    public native int setNativeApi(int index);

    public native int getNativeApi();

    @Override
    public native int getFramesPerBurst();

    public native int getSharingMode();

    public native int getPerformanceMode();

    public native int getSampleRate();

    public native int getFormat();

    public native int getChannelCount();

    public native int getDeviceId();

    public native int getSessionId();

    public native boolean isMMap();

    @Override
    public native long getCallbackCount();

    @Override
    public native long getFramesWritten();

    @Override
    public native long getFramesRead();

    @Override
    public native double getLatency();

    @Override
    public native int getState();

    public static native void setCallbackReturnStop(boolean b);

    public static native void setUseCallback(boolean checked);

    public static native void setCallbackSize(int callbackSize);
}
