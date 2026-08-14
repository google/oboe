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
import android.content.Context
import android.content.res.AssetManager
import android.net.Uri
import androidx.lifecycle.LiveData
import androidx.lifecycle.asLiveData
import com.google.oboe.samples.powerplay.effects.EffectsController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class DelegatingAudioEngine(
    private val context: Context,
    private val coroutineScope: CoroutineScope
) : AudioEngine {
    private var _activeEngineType = AudioEngineType.Oboe
    override val engineType: AudioEngineType get() = _activeEngineType

    private val oboeEngine = PowerPlayAudioPlayer()
    private val exoPlayerEngine = ExoPlayerAudioEngine(context)

    // Desired playback settings, mirrored to both engines. Tearing down and re-creating an
    // engine's native stream on switch resets these to defaults, so they are re-applied to the
    // newly active engine in switchEngine().
    private var desiredVolume = 1.0f
    private var desiredSpeed = 1.0f
    private var desiredPitch = 1.0f
    private var desiredLooping = false

    private val currentEngine: AudioEngine
        get() = when (_activeEngineType) {
            AudioEngineType.Oboe -> oboeEngine
            AudioEngineType.ExoPlayer -> exoPlayerEngine
        }

    private val _playerState = MutableStateFlow<PlayerState>(PlayerState.NoResultYet)
    override val playerStateFlow: StateFlow<PlayerState> get() = _playerState
    override fun getPlayerStateLive(): LiveData<PlayerState> = _playerState.asLiveData()

    private val _currentSongIndex = MutableStateFlow(0)
    override val currentSongIndexFlow: StateFlow<Int> get() = _currentSongIndex
    override fun getCurrentSongIndexLive(): LiveData<Int> = _currentSongIndex.asLiveData()
    override val currentSongIndex: Int get() = _currentSongIndex.value

    init {
        coroutineScope.launch {
            oboeEngine.playerStateFlow.collect { state ->
                if (_activeEngineType == AudioEngineType.Oboe) {
                    _playerState.value = state
                }
            }
        }
        coroutineScope.launch {
            oboeEngine.currentSongIndexFlow.collect { index ->
                if (_activeEngineType == AudioEngineType.Oboe) {
                    _currentSongIndex.value = index
                }
            }
        }

        coroutineScope.launch {
            exoPlayerEngine.playerStateFlow.collect { state ->
                if (_activeEngineType == AudioEngineType.ExoPlayer) {
                    _playerState.value = state
                }
            }
        }
        coroutineScope.launch {
            exoPlayerEngine.currentSongIndexFlow.collect { index ->
                if (_activeEngineType == AudioEngineType.ExoPlayer) {
                    _currentSongIndex.value = index
                }
            }
        }
    }

    fun switchEngine(type: AudioEngineType) {
        if (_activeEngineType == type) return

        val wasPlaying = _playerState.value == PlayerState.Playing
        val index = currentSongIndex
        val position = getPlaybackPositionMillis()

        currentEngine.stopPlaying(index)
        currentEngine.teardownAudioStream()

        _activeEngineType = type

        currentEngine.setupAudioStream()

        // setupAudioStream creates a fresh native stream/player at default settings; restore the
        // user's mirrored playback settings on the now-active engine.
        currentEngine.setVolume(desiredVolume)
        currentEngine.setPlaybackParameters(desiredSpeed, desiredPitch)
        currentEngine.setLooping(index, desiredLooping)

        _playerState.value = currentEngine.playerStateFlow.value
        _currentSongIndex.value = currentEngine.currentSongIndexFlow.value

        if (wasPlaying) {
            currentEngine.startPlaying(index)
            currentEngine.seekTo(position.toInt())
        }
    }

    override val currentPerformanceMode: OboePerformanceMode
        get() = currentEngine.currentPerformanceMode

    override val effectsController: EffectsController?
        get() = currentEngine.effectsController

    override fun setupAudioStream(channelCount: Int) {
        oboeEngine.setupAudioStream(channelCount)
        exoPlayerEngine.setupAudioStream(channelCount)
        _playerState.value = currentEngine.playerStateFlow.value
    }

    override fun startPlaying(index: Int, mode: OboePerformanceMode?) {
        currentEngine.startPlaying(index, mode)
    }

    override fun stopPlaying(index: Int) {
        currentEngine.stopPlaying(index)
    }

    override fun setLooping(index: Int, looping: Boolean) {
        desiredLooping = looping
        oboeEngine.setLooping(index, looping)
        exoPlayerEngine.setLooping(index, looping)
    }

    override fun setVolume(volume: Float) {
        desiredVolume = volume
        oboeEngine.setVolume(volume)
        exoPlayerEngine.setVolume(volume)
    }

    override fun seekTo(positionMillis: Int) {
        currentEngine.seekTo(positionMillis)
    }

    override fun getPlaybackPositionMillis(): Long {
        return currentEngine.getPlaybackPositionMillis()
    }

    override fun getDurationMillis(index: Int): Long {
        return currentEngine.getDurationMillis(index)
    }

    override fun getCurrentlyPlayingIndex(): Int {
        return currentEngine.getCurrentlyPlayingIndex()
    }

    override fun teardownAudioStream() {
        oboeEngine.teardownAudioStream()
        exoPlayerEngine.teardownAudioStream()
    }

    override fun loadFile(assetMgr: AssetManager, filename: String, id: Int): WavFileInfo? {
        // Oboe probes the WAV header and returns the metadata; mirror the source into ExoPlayer
        // and hand it the duration Oboe computed.
        val wavInfo = oboeEngine.loadFile(assetMgr, filename, id)
        if (wavInfo != null) {
            exoPlayerEngine.loadFile(assetMgr, filename, id)
            exoPlayerEngine.setTrackDuration(id, wavInfo.durationMs)
        }
        return wavInfo
    }

    override fun loadLocalFile(contentResolver: ContentResolver, uri: Uri, index: Int): WavFileInfo? {
        val wavInfo = oboeEngine.loadLocalFile(contentResolver, uri, index)
        if (wavInfo != null) {
            exoPlayerEngine.loadLocalFile(contentResolver, uri, index)
            exoPlayerEngine.setTrackDuration(index, wavInfo.durationMs)
        }
        return wavInfo
    }

    override fun removeSampleSource(index: Int): Boolean {
        val r1 = oboeEngine.removeSampleSource(index)
        val r2 = exoPlayerEngine.removeSampleSource(index)
        return r1 && r2
    }

    override fun setPlaybackParameters(speed: Float, pitch: Float): Boolean {
        desiredSpeed = speed
        desiredPitch = pitch
        val oboeResult = oboeEngine.setPlaybackParameters(speed, pitch)
        val exoResult = exoPlayerEngine.setPlaybackParameters(speed, pitch)
        // Report the result of the engine that is actually playing, so the inactive engine
        // (e.g. Oboe failing below API 37) doesn't veto a change the active engine applied.
        return if (_activeEngineType == AudioEngineType.Oboe) oboeResult else exoResult
    }

    override fun setMMapEnabled(enabled: Boolean) {
        oboeEngine.setMMapEnabled(enabled)
    }

    override fun isMMapEnabled(): Boolean {
        return oboeEngine.isMMapEnabled()
    }

    override fun isMMapSupported(): Boolean {
        return oboeEngine.isMMapSupported()
    }

    override fun setBufferSizeInFrames(bufferSizeInFrames: Int): Int {
        return oboeEngine.setBufferSizeInFrames(bufferSizeInFrames)
    }

    override fun getBufferCapacityInFrames(): Int {
        return oboeEngine.getBufferCapacityInFrames()
    }

    override fun isOffloaded(): Boolean {
        return oboeEngine.isOffloaded()
    }

    override fun getSessionId(): Int {
        return oboeEngine.getSessionId()
    }

    override fun updatePerformanceMode(mode: OboePerformanceMode) {
        oboeEngine.updatePerformanceMode(mode)
    }

    override fun setOffloadSchedulingEnabled(enabled: Boolean) {
        currentEngine.setOffloadSchedulingEnabled(enabled)
    }

    override fun isOffloadSchedulingEnabled(): Boolean {
        return currentEngine.isOffloadSchedulingEnabled()
    }
}
