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
