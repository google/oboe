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
import androidx.annotation.OptIn
import androidx.media3.common.util.UnstableApi
import androidx.media3.common.TrackSelectionParameters
import androidx.media3.common.MediaItem
import androidx.media3.common.PlaybackParameters
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import com.google.oboe.samples.powerplay.effects.EffectsController
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import java.util.concurrent.ConcurrentHashMap

class ExoPlayerAudioEngine(private val context: Context) : AudioEngine {
    private var exoPlayer: ExoPlayer? = null

    private val _playerState = MutableStateFlow<PlayerState>(PlayerState.NoResultYet)
    override val playerStateFlow: StateFlow<PlayerState> get() = _playerState
    override fun getPlayerStateLive(): LiveData<PlayerState> = _playerState.asLiveData()

    private val _currentSongIndex = MutableStateFlow(0)
    override val currentSongIndexFlow: StateFlow<Int> get() = _currentSongIndex
    override fun getCurrentSongIndexLive(): LiveData<Int> = _currentSongIndex.asLiveData()
    override val currentSongIndex: Int get() = _currentSongIndex.value

    override val currentPerformanceMode = OboePerformanceMode.None
    override val effectsController: EffectsController? = null
    override val engineType = AudioEngineType.ExoPlayer

    // Written from the background file-loading thread and read on the main thread, so use a
    // concurrent map to avoid corrupting the structure under concurrent access.
    private val playlistItems = ConcurrentHashMap<Int, MediaItem>()
    private val trackDurations = ConcurrentHashMap<Int, Long>()

    private var currentVolume = 1.0f
    private var currentPlaybackParameters = PlaybackParameters.DEFAULT
    private var offloadSchedulingEnabled = false

    private val playerListener = object : Player.Listener {
        override fun onPlaybackStateChanged(playbackState: Int) {
            updatePlayerState()
        }

        override fun onPlayWhenReadyChanged(playWhenReady: Boolean, reason: Int) {
            updatePlayerState()
        }
    }

    private fun updatePlayerState() {
        val player = exoPlayer ?: return
        val playbackState = player.playbackState
        val playWhenReady = player.playWhenReady

        when (playbackState) {
            Player.STATE_READY, Player.STATE_BUFFERING -> {
                _playerState.update { if (playWhenReady) PlayerState.Playing else PlayerState.Stopped }
            }
            Player.STATE_ENDED, Player.STATE_IDLE -> {
                _playerState.update { PlayerState.Stopped }
            }
        }
    }

    @OptIn(UnstableApi::class)
    override fun setupAudioStream(channelCount: Int) {
        if (exoPlayer == null) {
            exoPlayer = ExoPlayer.Builder(context).build().apply {
                addListener(playerListener)
                volume = currentVolume
                playbackParameters = currentPlaybackParameters
                applyOffloadPreferences(this)
            }
        }
        _playerState.update { PlayerState.Initialized }
    }

    /** Applies the current offload-scheduling preference to [player]. */
    @OptIn(UnstableApi::class)
    private fun applyOffloadPreferences(player: ExoPlayer) {
        val mode = if (offloadSchedulingEnabled) {
            TrackSelectionParameters.AudioOffloadPreferences.AUDIO_OFFLOAD_MODE_ENABLED
        } else {
            TrackSelectionParameters.AudioOffloadPreferences.AUDIO_OFFLOAD_MODE_DISABLED
        }
        val offloadPrefs = TrackSelectionParameters.AudioOffloadPreferences.Builder()
            .setAudioOffloadMode(mode)
            .build()
        player.trackSelectionParameters = player.trackSelectionParameters.buildUpon()
            .setAudioOffloadPreferences(offloadPrefs)
            .build()
    }

    override fun startPlaying(index: Int, mode: OboePerformanceMode?) {
        _currentSongIndex.update { index }
        val item = playlistItems[index]
        if (item != null) {
            setupAudioStream()
            exoPlayer?.let { player ->
                player.setMediaItem(item)
                player.prepare()
                player.play()
                _playerState.update { PlayerState.Playing }
            }
        } else {
            _playerState.update { PlayerState.Error }
        }
    }

    override fun stopPlaying(index: Int) {
        exoPlayer?.stop()
        _playerState.update { PlayerState.Stopped }
    }

    override fun setVolume(volume: Float) {
        currentVolume = volume
        exoPlayer?.volume = volume
    }

    override fun seekTo(positionMillis: Int) {
        exoPlayer?.seekTo(positionMillis.toLong())
    }

    override fun getPlaybackPositionMillis(): Long {
        return exoPlayer?.currentPosition ?: 0L
    }

    override fun getDurationMillis(index: Int): Long {
        return trackDurations[index] ?: 0L
    }

    override fun getCurrentlyPlayingIndex(): Int {
        val player = exoPlayer
        return if (player != null && player.isPlaying) _currentSongIndex.value else -1
    }

    override fun teardownAudioStream() {
        exoPlayer?.removeListener(playerListener)
        exoPlayer?.release()
        exoPlayer = null
    }

    // ExoPlayer probes audio properties only after preparing an item, so loadFile/loadLocalFile
    // just register the source URI here and return null; the WAV duration is supplied separately
    // via setTrackDuration() from the duration the Oboe engine computed.
    override fun loadFile(assetMgr: AssetManager, filename: String, id: Int): WavFileInfo? {
        playlistItems[id] = MediaItem.fromUri(Uri.parse("asset:///$filename"))
        return null
    }

    override fun loadLocalFile(contentResolver: ContentResolver, uri: Uri, index: Int): WavFileInfo? {
        playlistItems[index] = MediaItem.fromUri(uri)
        return null
    }

    /** Records the duration (probed by the Oboe engine) for the track at [index]. */
    fun setTrackDuration(index: Int, durationMs: Long) {
        trackDurations[index] = durationMs
    }

    override fun removeSampleSource(index: Int): Boolean {
        val existed = playlistItems.remove(index) != null
        trackDurations.remove(index)
        // The native Oboe player erases from a vector and the UI playlist uses removeAt(index),
        // both of which shift higher indices down by one. Mirror that here so the index-keyed
        // maps stay aligned with track positions.
        playlistItems.keys.filter { it > index }.sorted().forEach { key ->
            playlistItems[key - 1] = playlistItems.remove(key)!!
        }
        trackDurations.keys.filter { it > index }.sorted().forEach { key ->
            trackDurations[key - 1] = trackDurations.remove(key)!!
        }
        return existed
    }

    override fun setLooping(index: Int, looping: Boolean) {
        exoPlayer?.repeatMode = if (looping) Player.REPEAT_MODE_ONE else Player.REPEAT_MODE_OFF
    }

    override fun setPlaybackParameters(speed: Float, pitch: Float): Boolean {
        val params = PlaybackParameters(speed, pitch)
        currentPlaybackParameters = params
        exoPlayer?.playbackParameters = params
        return true
    }

    override fun setMMapEnabled(enabled: Boolean) {}
    override fun isMMapEnabled() = false
    override fun isMMapSupported() = false
    override fun setBufferSizeInFrames(bufferSizeInFrames: Int) = bufferSizeInFrames
    override fun getBufferCapacityInFrames() = 0
    override fun isOffloaded() = false
    override fun getSessionId() = 0
    override fun updatePerformanceMode(mode: OboePerformanceMode) {}

    @OptIn(UnstableApi::class)
    override fun setOffloadSchedulingEnabled(enabled: Boolean) {
        offloadSchedulingEnabled = enabled
        exoPlayer?.let { applyOffloadPreferences(it) }
    }

    override fun isOffloadSchedulingEnabled(): Boolean = offloadSchedulingEnabled
}
