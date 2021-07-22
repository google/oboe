package com.google.oboe.samples.soundboard;

public class NoteListener implements TileListener {
    private native void noteOn(long engineHandle, int noteIndex);
    private native void noteOff(long engineHandle, int noteIndex);

    long mEngineHandle;

    public NoteListener(long engineHandle) {
        mEngineHandle = engineHandle;
    }

    public void onTileOn(int index) {
        noteOn(mEngineHandle, index);
    }

    public void onTileOff(int index) {
        noteOff(mEngineHandle, index);
    }
}
