package com.google.oboe.samples.powerplay.effects

import android.media.audiofx.Equalizer
import android.util.Log

data class EqualizerBand(
    val id: Short,
    val centerFreqHz: Int,
    val minLevelmB: Short,
    val maxLevelmB: Short,
    var currentLevelmB: Short
)

class EqualizerManager(sessionId: Int) {
    private val TAG = "EqualizerManager"
    private var equalizer: Equalizer? = null
    private val _bands = mutableListOf<EqualizerBand>()

    init {
        try {
            equalizer = Equalizer(0, sessionId).apply {
                enabled = true
                // Force all bands to 0
                val numBands = numberOfBands
                for (i in 0 until numBands) {
                    setBandLevel(i.toShort(), 0)
                }
            }
            Log.i(TAG, "Equalizer created for session $sessionId")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create Equalizer", e)
        }
    }

    val isAvailable: Boolean get() = equalizer != null

    fun getBands(): List<EqualizerBand> {
        if (_bands.isEmpty() && equalizer != null) {
            val eq = equalizer!!
            val numBands = eq.numberOfBands
            val range = eq.bandLevelRange
            for (i in 0 until numBands) {
                val bandId = i.toShort()
                _bands.add(
                    EqualizerBand(
                        id = bandId,
                        centerFreqHz = eq.getCenterFreq(bandId) / 1000,
                        minLevelmB = range[0],
                        maxLevelmB = range[1],
                        currentLevelmB = 0
                    )
                )
            }
        }
        return _bands
    }

    fun setBandLevel(band: Short, level: Short) {
        equalizer?.setBandLevel(band, level)
        _bands.find { it.id == band }?.currentLevelmB = level
    }

    fun reset() {
        _bands.forEach { band ->
            band.currentLevelmB = 0
            equalizer?.setBandLevel(band.id, 0)
        }
    }

    fun release() {
        equalizer?.release()
        equalizer = null
    }
}
