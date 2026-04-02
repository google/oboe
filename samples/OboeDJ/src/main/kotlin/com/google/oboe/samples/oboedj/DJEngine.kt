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

package com.google.oboe.samples.oboedj

import android.content.res.AssetManager
import java.io.IOException

class DJEngine {
    companion object {
        init {
            System.loadLibrary("oboedj")
        }
    }

    fun init() = initNative()
    fun start() = startNative()
    fun stop() = stopNative()
    
    fun loadTrack(assetManager: AssetManager, filename: String, deckIndex: Int) {
        try {
            assetManager.open(filename).use { inputStream ->
                val bytes = inputStream.readBytes()
                loadTrackNative(bytes, deckIndex)
            }
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    fun setSpeed(deckIndex: Int, speed: Float) = setSpeedNative(deckIndex, speed)
    fun setPlaying(deckIndex: Int, isPlaying: Boolean) = setPlayingNative(deckIndex, isPlaying)
    fun setCrossfader(position: Float) = setCrossfaderNative(position)

    // JNI Methods
    private external fun initNative()
    private external fun startNative()
    private external fun stopNative()
    private external fun loadTrackNative(trackBytes: ByteArray, deckIndex: Int)
    private external fun setSpeedNative(deckIndex: Int, speed: Float)
    private external fun setPlayingNative(deckIndex: Int, isPlaying: Boolean)
    private external fun setCrossfaderNative(position: Float)
}
