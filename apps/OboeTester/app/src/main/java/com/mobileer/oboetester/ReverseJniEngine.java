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
            mNativeEngineHandle = native_createEngine();
            Log.i(TAG, "Created native engine with handle: " + mNativeEngineHandle);
        }
    }

    public void start(int bufferSizeInBursts) {
        if (mNativeEngineHandle != 0) {
            native_startEngine(mNativeEngineHandle, bufferSizeInBursts);
            Log.i(TAG, "Started native engine.");
        }
    }

    public void stop() {
        if (mNativeEngineHandle != 0) {
            native_stopEngine(mNativeEngineHandle);
            Log.i(TAG, "Stopped native engine.");
        }
    }

    public void destroy() {
        if (mNativeEngineHandle != 0) {
            native_deleteEngine(mNativeEngineHandle);
            mNativeEngineHandle = 0;
            Log.i(TAG, "Destroyed native engine.");
        }
    }

    public void setBufferSizeInBursts(int bufferSizeInBursts) {
        if (mNativeEngineHandle != 0) {
            native_setBufferSizeInBursts(mNativeEngineHandle, bufferSizeInBursts);
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
        // Simple sine wave generator
        for (int i = 0; i < numFrames; i++) {
            audioData[i] = (float) Math.sin(mPhase);
            mPhase += mPhaseIncrement;
            if (mPhase > 2 * Math.PI) {
                mPhase -= 2 * Math.PI;
            }
        }
        mXRunCount = totalXRunCount;
        Log.i(TAG, "XRun count: " + mXRunCount);
        try {
            Thread.sleep(mSleepDurationUs / 1000, (mSleepDurationUs % 1000) * 1000);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    // Native methods that are implemented in jni-bridge.cpp
    private native long native_createEngine();
    private native void native_startEngine(long enginePtr, int bufferSizeInBursts);
    private native void native_stopEngine(long enginePtr);
    private native void native_deleteEngine(long enginePtr);
    private native void native_setBufferSizeInBursts(long enginePtr, int bufferSizeInBursts);
}
