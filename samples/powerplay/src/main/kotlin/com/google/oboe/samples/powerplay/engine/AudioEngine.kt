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

package com.google.oboe.samples.powerplay.engine

import android.content.ContentResolver
import android.content.res.AssetManager
import android.net.Uri
import androidx.lifecycle.LiveData
import com.google.oboe.samples.powerplay.effects.EffectsController
import kotlinx.coroutines.flow.StateFlow

interface AudioEngine {
    val currentSongIndex: Int
    val currentPerformanceMode: OboePerformanceMode
    val effectsController: EffectsController?
    val engineType: AudioEngineType

    val playerStateFlow: StateFlow<PlayerState>
    val currentSongIndexFlow: StateFlow<Int>

    fun getPlayerStateLive(): LiveData<PlayerState>
    fun getCurrentSongIndexLive(): LiveData<Int>

    fun setupAudioStream(channelCount: Int = 2)
    fun startPlaying(index: Int, mode: OboePerformanceMode? = null)
    fun stopPlaying(index: Int)
    fun setLooping(index: Int, looping: Boolean)
    fun setVolume(volume: Float)
    fun seekTo(positionMillis: Int)
    fun getPlaybackPositionMillis(): Long
    fun getDurationMillis(index: Int): Long
    fun getCurrentlyPlayingIndex(): Int
    fun teardownAudioStream()

    fun loadFile(assetMgr: AssetManager, filename: String, id: Int): WavFileInfo?
    fun loadLocalFile(contentResolver: ContentResolver, uri: Uri, index: Int): WavFileInfo?
    fun removeSampleSource(index: Int): Boolean
    fun setPlaybackParameters(speed: Float, pitch: Float): Boolean

    // Oboe-specific properties/methods
    fun setMMapEnabled(enabled: Boolean)
    fun isMMapEnabled(): Boolean
    fun isMMapSupported(): Boolean
    fun setBufferSizeInFrames(bufferSizeInFrames: Int): Int
    fun getBufferCapacityInFrames(): Int
    fun isOffloaded(): Boolean
    fun getSessionId(): Int
    fun updatePerformanceMode(mode: OboePerformanceMode)

    // ExoPlayer-specific properties/methods
    fun setOffloadSchedulingEnabled(enabled: Boolean)
    fun isOffloadSchedulingEnabled(): Boolean
}
