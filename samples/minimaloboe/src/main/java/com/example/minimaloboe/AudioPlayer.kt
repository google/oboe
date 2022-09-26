package com.example.minimaloboe

class AudioPlayer {
    var mStarted = false;
    fun startAudio() {
        mStarted = true;
    }
    fun stopAudio() {
        mStarted = false;
    }
}