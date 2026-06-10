/*
 * Copyright 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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

    val isAvailable: Boolean get() = loudnessEnhancer != null

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
