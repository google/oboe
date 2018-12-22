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

import java.io.IOException;

/**
 * Base class for any audio input or output.
 */
public abstract class AudioStreamBase {

    private StreamConfiguration mRequestedStreamConfiguration;
    private StreamConfiguration mActualStreamConfiguration;

    private int mBufferSizeInFrames;

    public StreamStatus getStreamStatus() {
        StreamStatus status = new StreamStatus();
        status.bufferSize = getBufferSizeInFrames();
        status.xRunCount = getUnderrunCount();
        status.framesRead = getFramesRead();
        status.framesWritten = getFramesWritten();
        status.callbackCount = getCallbackCount();
        status.latency = getLatency();
        status.state = getState();
        return status;
    }

    /**
     * Changes dynamic at run-time.
     */
    public static class StreamStatus {
        public int bufferSize;
        public int xRunCount;
        public long framesWritten;
        public long framesRead;
        public double latency; // msec
        public int state;
        public long callbackCount;
    }

    public void open(StreamConfiguration requestedConfiguration,
                     StreamConfiguration actualConfiguration,
                     int bufferSizeInFrames) throws IOException {
        mRequestedStreamConfiguration = requestedConfiguration;
        mActualStreamConfiguration = actualConfiguration;
        mBufferSizeInFrames = bufferSizeInFrames;
    }

    public abstract boolean isInput();

    public abstract void start() throws IOException;

    public abstract void pause() throws IOException;

    public abstract void stop() throws IOException;

    public void startPlayback() throws IOException {}

    public void stopPlayback() throws IOException {}

    public abstract int write(float[] buffer, int offset, int length);

    public abstract void close();

    public int getChannelCount() {
        return mActualStreamConfiguration.getChannelCount();
    }

    public int getFramesPerBurst() {
        return mActualStreamConfiguration.getFramesPerBurst();
    }

    public int getBufferCapacityInFrames() {
        return mBufferSizeInFrames;
    }

    public int getBufferSizeInFrames() {
        return mBufferSizeInFrames;
    }

    public int setBufferSizeInFrames(int bufferSize) {
        throw new UnsupportedOperationException("bufferSize cannot be changed");
    }

    public long getCallbackCount() { return -1; }

    public long getFramesWritten() { return -1; }

    public long getFramesRead() { return -1; }

    public double getLatency() { return -1.0; }

    public int getState() { return -1; }

    public boolean isThresholdSupported() {
        return false;
    }

//    public boolean isLowLatencySupported() {
//        return false;
//    }

    public void setAmplitude(double amplitude) {}

    public int getUnderrunCount() {
        return 0;
    }

//    public boolean isUnderrunCountSupported() {
//        return false;
//    }
}
