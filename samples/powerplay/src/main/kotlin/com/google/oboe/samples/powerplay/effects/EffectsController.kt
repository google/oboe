package com.google.oboe.samples.powerplay.effects

import android.media.audiofx.AudioEffect
import android.media.audiofx.BassBoost
import android.media.audiofx.Equalizer
import android.media.audiofx.LoudnessEnhancer

import android.util.Log
import java.util.UUID

class EffectsController {
    private val TAG = "EffectsController"
    
    var equalizer: EqualizerManager? = null
        private set
    var bassBoost: BassBoostManager? = null
        private set
    var loudness: LoudnessManager? = null
        private set
        
    fun initialize(sessionId: Int) {
        if (sessionId <= 0) {
            Log.w(TAG, "Invalid session ID $sessionId, cannot initialize effects")
            return
        }
        
        Log.i(TAG, "Initializing effects for session $sessionId")
        
        if (isEffectSupported(Equalizer.EFFECT_TYPE_EQUALIZER)) {
            equalizer = EqualizerManager(sessionId)
        } else {
            Log.w(TAG, "Equalizer not supported")
        }
        
        if (isEffectSupported(BassBoost.EFFECT_TYPE_BASS_BOOST)) {
            bassBoost = BassBoostManager(sessionId)
        } else {
            Log.w(TAG, "Bass Boost not supported")
        }
        
        if (isEffectSupported(LoudnessEnhancer.EFFECT_TYPE_LOUDNESS_ENHANCER)) {
            loudness = LoudnessManager(sessionId)
        } else {
            Log.w(TAG, "Loudness Enhancer not supported")
        }
    }
    
    private fun isEffectSupported(effectTypeUuid: UUID): Boolean {
        val descriptors = AudioEffect.queryEffects()
        for (descriptor in descriptors) {
            if (descriptor.type == effectTypeUuid) {
                return true
            }
        }
        return false
    }
    
    fun release() {
        Log.i(TAG, "Releasing effects")
        equalizer?.release()
        bassBoost?.release()
        loudness?.release()
        equalizer = null
        bassBoost = null
        loudness = null
    }

    enum class EffectType {
        EQUALIZER, BASS_BOOST, LOUDNESS
    }

    fun getSupportedEffects(): List<EffectType> {
        val list = mutableListOf<EffectType>()
        if (equalizer?.isAvailable == true) list.add(EffectType.EQUALIZER)
        if (bassBoost?.isAvailable == true) list.add(EffectType.BASS_BOOST)
        if (loudness?.isAvailable == true) list.add(EffectType.LOUDNESS)
        return list
    }
}
