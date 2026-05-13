package com.google.oboe.samples.powerplay.effects

import android.media.audiofx.Virtualizer
import android.util.Log

class VirtualizerManager(private val sessionId: Int) {
    private val TAG = "VirtualizerManager"
    private var virtualizer: Virtualizer? = null

    init {
        try {
            virtualizer = Virtualizer(0, sessionId).apply {
                enabled = true
            }
            Log.i(TAG, "Virtualizer created for session $sessionId")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create Virtualizer", e)
        }
    }

    fun setStrength(strength: Short) {
        if (virtualizer?.strengthSupported == true) {
            virtualizer?.setStrength(strength)
        }
    }

    fun getStrength(): Short = virtualizer?.roundedStrength ?: 0

    fun setEnabled(enable: Boolean) {
        virtualizer?.enabled = enable
    }

    fun isEnabled(): Boolean = virtualizer?.enabled ?: false

    fun release() {
        virtualizer?.release()
        virtualizer = null
    }
}
