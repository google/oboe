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
 * Implementation of an AudioStreamBase using Oboe.
 */

abstract class OboeAudioStream extends AudioStreamBase {
    private static final int INVALID_STREAM_INDEX = -1;
    int streamIndex = INVALID_STREAM_INDEX;

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
        int result = openNative(requestedConfiguration.getNativeApi(),
                requestedConfiguration.getSampleRate(),
                requestedConfiguration.getChannelCount(),
                requestedConfiguration.getFormat(),
                requestedConfiguration.getSharingMode(),
                requestedConfiguration.getPerformanceMode(),
                requestedConfiguration.getInputPreset(),
                requestedConfiguration.getDeviceId(),
                requestedConfiguration.getSessionId(),
                requestedConfiguration.getFramesPerBurst(),
                requestedConfiguration.getChannelConversionAllowed(),
                requestedConfiguration.getFormatConversionAllowed(),
                requestedConfiguration.getRateConversionQuality(),
                requestedConfiguration.isMMap(),
                isInput()
        );
        if (result < 0) {
            streamIndex = INVALID_STREAM_INDEX;
            throw new IOException("Open failed! result = " + result);
        } else {
            streamIndex = result;
        }
        actualConfiguration.setNativeApi(getNativeApi());
        actualConfiguration.setSampleRate(getSampleRate());
        actualConfiguration.setSharingMode(getSharingMode());
        actualConfiguration.setPerformanceMode(getPerformanceMode());
        actualConfiguration.setInputPreset(getInputPreset());
        actualConfiguration.setFramesPerBurst(getFramesPerBurst());
        actualConfiguration.setBufferCapacityInFrames(getBufferCapacityInFrames());
        actualConfiguration.setChannelCount(getChannelCount());
        actualConfiguration.setDeviceId(getDeviceId());
        actualConfiguration.setSessionId(getSessionId());
        actualConfiguration.setFormat(getFormat());
        actualConfiguration.setMMap(isMMap());
        actualConfiguration.setDirection(isInput()
                ? StreamConfiguration.DIRECTION_INPUT
                : StreamConfiguration.DIRECTION_OUTPUT);
    }

    private native int openNative(
            int nativeApi,
            int sampleRate,
            int channelCount,
            int format,
            int sharingMode,
            int performanceMode,
            int inputPreset,
            int deviceId,
            int sessionId,
            int framesPerRead,
            boolean channelConversionAllowed,
            boolean formatConversionAllowed,
            int rateConversionQuality,
            boolean isMMap,
            boolean isInput);

    @Override
    public void close() {
        if (streamIndex >= 0) {
            close(streamIndex);
            streamIndex = INVALID_STREAM_INDEX;
        }
    }
    public native void close(int streamIndex);

    @Override
    public int getBufferCapacityInFrames() {
        return getBufferCapacityInFrames(streamIndex);
    }
    private native int getBufferCapacityInFrames(int streamIndex);

    @Override
    public int getBufferSizeInFrames() {
        return getBufferSizeInFrames(streamIndex);
    }
    private native int getBufferSizeInFrames(int streamIndex);

    @Override
    public boolean isThresholdSupported() {
        return true;
    }

    @Override
    public int setBufferSizeInFrames(int thresholdFrames) {
        return setBufferSizeInFrames(streamIndex, thresholdFrames);
    }
    private native int setBufferSizeInFrames(int streamIndex, int thresholdFrames);

    public int getNativeApi() {
        return getNativeApi(streamIndex);
    }
    public native int getNativeApi(int streamIndex);

    @Override
    public int getFramesPerBurst() {
        return getFramesPerBurst(streamIndex);
    }
    public native int getFramesPerBurst(int streamIndex);

    public int getSharingMode() {
        return getSharingMode(streamIndex);
    }
    public native int getSharingMode(int streamIndex);

    public int getPerformanceMode() {
        return getPerformanceMode(streamIndex);
    }
    public native int getPerformanceMode(int streamIndex);

    public int getInputPreset() {
        return getInputPreset(streamIndex);
    }
    public native int getInputPreset(int streamIndex);

    public int getSampleRate() {
        return getSampleRate(streamIndex);
    }
    public native int getSampleRate(int streamIndex);

    public int getFormat() {
        return getFormat(streamIndex);
    }
    public native int getFormat(int streamIndex);

    public int getChannelCount() {
        return getChannelCount(streamIndex);
    }
    public native int getChannelCount(int streamIndex);

    public int getDeviceId() {
        return getDeviceId(streamIndex);
    }
    public native int getDeviceId(int streamIndex);

    public int getSessionId() {
        return getSessionId(streamIndex);
    }
    public native int getSessionId(int streamIndex);

    public boolean isMMap() {
        return isMMap(streamIndex);
    }
    public native boolean isMMap(int streamIndex);

    @Override
    public long getCallbackCount() {
        return getCallbackCount(streamIndex);
    }
    public native long getCallbackCount(int streamIndex);

    @Override
    public long getFramesWritten() {
        return getFramesWritten(streamIndex);
    }
    public native long getFramesWritten(int streamIndex);

    @Override
    public long getFramesRead() {
        return getFramesRead(streamIndex);
    }
    public native long getFramesRead(int streamIndex);

    @Override
    public int getXRunCount() {
        return getXRunCount(streamIndex);
    }
    public native int getXRunCount(int streamIndex);

    @Override
    public double getLatency() {
        return getLatency(streamIndex);
    }
    public native double getLatency(int streamIndex);

    @Override
    public double getCpuLoad() {
        return getCpuLoad(streamIndex);
    }
    public native double getCpuLoad(int streamIndex);

    @Override
    public native void setWorkload(double workload);

    @Override
    public int getState() {
        return getState(streamIndex);
    }
    public native int getState(int streamIndex);

    public static native void setCallbackReturnStop(boolean b);

    public static native void setUseCallback(boolean checked);

    public static native void setCallbackSize(int callbackSize);

    public static native int getOboeVersionNumber();
}
