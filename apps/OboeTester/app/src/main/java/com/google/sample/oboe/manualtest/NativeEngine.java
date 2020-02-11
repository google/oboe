package com.google.sample.oboe.manualtest;

public class NativeEngine {

    static native boolean isMMapSupported();

    static native boolean isMMapExclusiveSupported();

    static native void setWorkaroundsEnabled(boolean enabled);
}
