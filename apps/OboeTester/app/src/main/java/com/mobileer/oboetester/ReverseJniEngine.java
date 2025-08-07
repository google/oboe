/*
 * Copyright 2025 The Android Open Source Project
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

import android.util.Log;

public class ReverseJniEngine {

    private static final String TAG = "ReverseJniEngine";
    private long mNativeEngineHandle = 0;
    private double mPhase = 0.0;
    private double mPhaseIncrement = 0.0;
    private final int mSampleRate = 48000;
    private final double mFrequency = 440.0; // A4 tone
    private int mXRunCount = 0;
    private int mSleepDurationUs = 0;

    // Load the native library
    static {
        System.loadLibrary("oboetester");
    }

    public ReverseJniEngine() {
        // Calculate the phase increment for our sine wave generator
        mPhaseIncrement = 2 * Math.PI * mFrequency / mSampleRate;
    }

    public void create() {
        if (mNativeEngineHandle == 0) {
            mNativeEngineHandle = createEngine();
            Log.i(TAG, "Created native engine with handle: " + mNativeEngineHandle);
        }
    }

    public void start(int bufferSizeInBursts) {
        if (mNativeEngineHandle != 0) {
            startEngine(mNativeEngineHandle, bufferSizeInBursts);
            Log.i(TAG, "Started native engine.");
        }
    }

    public void stop() {
        if (mNativeEngineHandle != 0) {
            stopEngine(mNativeEngineHandle);
            Log.i(TAG, "Stopped native engine.");
        }
    }

    public void destroy() {
        if (mNativeEngineHandle != 0) {
            deleteEngine(mNativeEngineHandle);
            mNativeEngineHandle = 0;
            Log.i(TAG, "Destroyed native engine.");
        }
    }

    public void setBufferSizeInBursts(int bufferSizeInBursts) {
        if (mNativeEngineHandle != 0) {
            setBufferSizeInBursts(mNativeEngineHandle, bufferSizeInBursts);
            Log.i(TAG, "SetBufferSizeInBursts:" + bufferSizeInBursts);
        }
    }

    public int getXRunCount() {
        return mXRunCount;
    }

    public void setSleepDurationUs(int sleepDurationUs) {
        mSleepDurationUs = sleepDurationUs;
    }

    @SuppressWarnings("unused") // Called from JNI
    private void onAudioReady(float[] audioData, int numFrames, int totalXRunCount) {
        if (audioData == null) {
            Log.e(TAG, "Audio data is null");
        }
        // Simple sine wave generator
        for (int i = 0; i < numFrames; i++) {
            audioData[i] = (float) Math.sin(mPhase);
            mPhase += mPhaseIncrement;
            if (mPhase > 2 * Math.PI) {
                mPhase -= 2 * Math.PI;
            }
        }
        mXRunCount = totalXRunCount;
        //Log.i(TAG, "XRun count: " + mXRunCount);
        try {
            // First parameter is milliseconds, second is the remaining part in nanoseconds
            Thread.sleep(mSleepDurationUs / 1000, (mSleepDurationUs % 1000) * 1000);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    // Native methods that are implemented in jni-bridge.cpp
    private native long createEngine();
    private native void startEngine(long enginePtr, int bufferSizeInBursts);
    private native void stopEngine(long enginePtr);
    private native void deleteEngine(long enginePtr);
    private native void setBufferSizeInBursts(long enginePtr, int bufferSizeInBursts);
}
