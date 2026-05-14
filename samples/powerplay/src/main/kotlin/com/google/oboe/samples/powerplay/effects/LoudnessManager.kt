package com.google.oboe.samples.powerplay.effects

import android.media.audiofx.LoudnessEnhancer
import android.util.Log

class LoudnessManager(sessionId: Int) {
    private val TAG = "LoudnessManager"
    private var loudnessEnhancer: LoudnessEnhancer? = null

    init {
        try {
            loudnessEnhancer = LoudnessEnhancer(sessionId).apply {
                enabled = true
            }
            Log.i(TAG, "LoudnessEnhancer created for session $sessionId")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create LoudnessEnhancer", e)
        }
    }

    fun setTargetGain(gainmB: Int) {
        loudnessEnhancer?.setTargetGain(gainmB)
    }

    fun getTargetGain(): Int = loudnessEnhancer?.targetGain?.toInt() ?: 0

    fun setEnabled(enable: Boolean) {
        loudnessEnhancer?.enabled = enable
    }

    fun isEnabled(): Boolean = loudnessEnhancer?.enabled ?: false

    fun release() {
        loudnessEnhancer?.release()
        loudnessEnhancer = null
    }
}
