package com.google.oboe.samples.powerplay.effects

import android.media.audiofx.BassBoost
import android.util.Log

class BassBoostManager(sessionId: Int) {
    private val TAG = "BassBoostManager"
    private var bassBoost: BassBoost? = null

    init {
        try {
            bassBoost = BassBoost(0, sessionId).apply {
                enabled = true
            }
            Log.i(TAG, "BassBoost created for session $sessionId")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create BassBoost", e)
        }
    }

    val isAvailable: Boolean get() = bassBoost != null

    fun setStrength(strength: Short) {
        if (bassBoost?.strengthSupported == true) {
            bassBoost?.setStrength(strength)
        }
    }

    fun getStrength(): Short = bassBoost?.roundedStrength ?: 0

    fun release() {
        bassBoost?.release()
        bassBoost = null
    }
}
