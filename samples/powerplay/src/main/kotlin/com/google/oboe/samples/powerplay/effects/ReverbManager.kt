package com.google.oboe.samples.powerplay.effects

import android.media.audiofx.EnvironmentalReverb
import android.util.Log

class ReverbManager(private val sessionId: Int) {
    private val TAG = "ReverbManager"
    private var reverb: EnvironmentalReverb? = null

    enum class Preset {
        NONE, SMALL_ROOM, MEDIUM_ROOM, LARGE_HALL, PLATE
    }

    private var currentPreset = Preset.NONE

    init {
        try {
            reverb = EnvironmentalReverb(0, sessionId).apply {
                enabled = false // Start disabled
            }
            Log.i(TAG, "EnvironmentalReverb created for session $sessionId")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create EnvironmentalReverb", e)
        }
    }

    fun setPreset(preset: Preset) {
        currentPreset = preset
        if (reverb == null) return

        if (preset == Preset.NONE) {
            reverb?.enabled = false
            return
        }

        reverb?.enabled = true
        when (preset) {
            Preset.SMALL_ROOM -> {
                reverb?.decayTime = 500
                reverb?.roomLevel = -1000
            }
            Preset.MEDIUM_ROOM -> {
                reverb?.decayTime = 1000
                reverb?.roomLevel = -500
            }
            Preset.LARGE_HALL -> {
                reverb?.decayTime = 3000
                reverb?.roomLevel = 0
            }
            Preset.PLATE -> {
                reverb?.decayTime = 1500
                reverb?.roomLevel = -200
            }
            else -> {}
        }
    }

    fun getPreset(): Preset = currentPreset

    fun setVolume(levelmB: Short) {
        reverb?.reverbLevel = levelmB
    }

    fun getVolume(): Short = reverb?.reverbLevel ?: -9000

    fun setEnabled(enable: Boolean) {
        reverb?.enabled = enable
    }

    fun isEnabled(): Boolean = reverb?.enabled ?: false

    fun release() {
        reverb?.release()
        reverb = null
    }
}
