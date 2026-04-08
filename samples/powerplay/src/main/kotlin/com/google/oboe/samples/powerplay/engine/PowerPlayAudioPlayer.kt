/*
 * Copyright 2025 The Android Open Source Project
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
import android.util.Log
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.asLiveData
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.update

class PowerPlayAudioPlayer() : DefaultLifecycleObserver {
    private var _playerState = MutableStateFlow<PlayerState>(PlayerState.NoResultYet)
    fun getPlayerStateLive() = _playerState.asLiveData()

    private var _currentSongIndex = MutableStateFlow(0)
    fun getCurrentSongIndexLive() = _currentSongIndex.asLiveData()
    val currentSongIndex: Int get() = _currentSongIndex.value

    private var _currentPerformanceMode = OboePerformanceMode.None
    val currentPerformanceMode: OboePerformanceMode get() = _currentPerformanceMode

    /**
     * Native passthrough functions
     */
    fun setupAudioStream(channelCount: Int = NUM_PLAY_CHANNELS) {
        setupAudioStreamNative(channelCount)
        _playerState.update { PlayerState.Initialized }
    }

    fun startPlaying(index: Int, mode: OboePerformanceMode?) {
        val actualMode = mode ?: _currentPerformanceMode
        _currentPerformanceMode = actualMode
        _currentSongIndex.update { index }

        startPlayingNative(index, actualMode)
        _playerState.update { PlayerState.Playing }
    }

    fun stopPlaying(index: Int) {
        stopPlayingNative(index)
        _playerState.update { PlayerState.Stopped }
    }

    fun updatePerformanceMode(mode: OboePerformanceMode) {
        _currentPerformanceMode = mode
        updatePerformanceModeNative(mode)
    }

    fun setLooping(index: Int, looping: Boolean) = setLoopingNative(index, looping)
    fun teardownAudioStream() = teardownAudioStreamNative()
    fun unloadAssets() = unloadAssetsNative()

    /**
     * Loads the file into memory from assets
     */
    fun loadFile(assetMgr: AssetManager, filename: String, id: Int) {
        val assetFD = assetMgr.openFd(filename)
        val stream = assetFD.createInputStream()
        val len = assetFD.getLength().toInt()
        val bytes = ByteArray(len)

        stream.read(bytes, 0, len)
        loadAssetNative(bytes, id)
        assetFD.close()
    }

    /**
     * Probes a WAV file's audio properties without loading sample data.
     * @param wavBytes The raw bytes of the WAV file
     * @return WavFileInfo containing parsed audio properties, or null on failure
     */
    fun getWavFileInfo(wavBytes: ByteArray): WavFileInfo? {
        return try {
            val info = getWavFileInfoNative(wavBytes)
            if (info.size != 6) return null

            val sampleRate = info[0]
            val numChannels = info[1]
            val bitsPerSample = info[2]
            val encoding = info[3]
            val numFrames = (info[4].toLong() shl 32) or (info[5].toLong() and 0xFFFFFFFFL)
            val durationMs = if (sampleRate > 0) (numFrames * 1000L) / sampleRate else 0L

            WavFileInfo(
                sampleRate = sampleRate,
                numChannels = numChannels,
                bitsPerSample = bitsPerSample,
                encoding = encoding,
                numSampleFrames = numFrames,
                durationMs = durationMs
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse WAV file info", e)
            null
        }
    }

    /**
     * Loads a local WAV file from a content URI.
     * @param contentResolver The ContentResolver to read the URI
     * @param uri The content URI of the WAV file
     * @param index The sample source index to load into
     * @return WavFileInfo if successful, null on failure
     */
    fun loadLocalFile(contentResolver: ContentResolver, uri: Uri, index: Int): WavFileInfo? {
        return try {
            val inputStream = contentResolver.openInputStream(uri)
                ?: throw IllegalStateException("Cannot open input stream for URI: $uri")

            val bytes = inputStream.use { it.readBytes() }

            // Check 256MB limit
            if (bytes.size > MAX_FILE_SIZE_BYTES) {
                Log.e(TAG, "File too large: ${bytes.size} bytes (max: $MAX_FILE_SIZE_BYTES)")
                return null
            }

            val wavInfo = getWavFileInfo(bytes) ?: return null

            // Load the sample data
            loadAssetNative(bytes, index)

            Log.i(TAG, "Loaded local file: ${wavInfo.sampleRate}Hz, " +
                    "${wavInfo.numChannels}ch, ${wavInfo.bitsPerSample}bit, " +
                    "${wavInfo.encodingName}, ${wavInfo.durationMs}ms")

            wavInfo
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load local file", e)
            null
        }
    }

    /**
     * Removes a sample source at the given index.
     * @param index The index of the sample source to remove
     * @return true if successfully removed
     */
    fun removeSampleSource(index: Int): Boolean = removeSampleSourceNative(index)

    /**
     * Sets whether the audio stream should use MMap audio.
     * @param enabled True to enable MMap, false to disable.
     */
    fun setMMapEnabled(enabled: Boolean) = setMMapEnabledNative(enabled)

    /**
     * Checks if MMap is currently enabled.
     * @return True if MMap is enabled, false otherwise.
     */
    fun isMMapEnabled(): Boolean = isMMapEnabledNative()

    /**
     * Checks if MMap is supported by the current device.
     * @return True if MMap is supported, false otherwise.
     */
    fun isMMapSupported(): Boolean = isMMapSupportedNative()

    /**
     * Sets the buffer size in frames for the audio stream.
     *
     * Lower buffer sizes provide lower latency but increase the risk of audio glitches (underruns).
     * This can only be set if the user is in PCM Offload mode.
     *
     * @param bufferSizeInFrames The requested buffer size in frames.
     * @return The actual buffer size set by the native audio engine.
     */
    fun setBufferSizeInFrames(bufferSizeInFrames: Int): Int = setBufferSizeInFramesNative(bufferSizeInFrames)

    fun getBufferCapacityInFrames(): Int = getBufferCapacityInFramesNative()

    /**
     * Sets the playback volume (gain) for the audio stream.
     *
     * @param volume Volume level from 0.0 (mute) to 1.0 (full volume)
     */
    fun setVolume(volume: Float) = setVolumeNative(volume)

    /**
     * Checks if the current audio stream is using PCM Offload.
     *
     * @return true if offload is active, false otherwise
     */
    fun isOffloaded(): Boolean = isOffloadedNative()

    /**
     * Gets the index of the currently playing track.
     *
     * @return Track index (0-based) or -1 if nothing is playing
     */
    fun getCurrentlyPlayingIndex(): Int = getCurrentlyPlayingIndexNative()

    /**
     * Gets the current playback position in milliseconds.
     */
    fun getPlaybackPositionMillis(): Long = getPlaybackPositionMillisNative()

    /**
     * Seeks to a specific position in milliseconds.
     */
    fun seekTo(positionMillis: Int) = seekToNative(positionMillis)

    /**
     * Gets the duration of the track at the specified index in milliseconds.
     */
    fun getDurationMillis(index: Int): Long = getDurationMillisNative(index)

    /**
     * Native functions.
     * Load the library containing the native code including the JNI functions.
     */
    init {
        System.loadLibrary("powerplay")
    }

    private external fun setupAudioStreamNative(numChannels: Int)
    private external fun startAudioStreamNative(): Int
    private external fun teardownAudioStreamNative(): Int
    private external fun loadAssetNative(wavBytes: ByteArray, index: Int)
    private external fun unloadAssetsNative()
    private external fun getOutputResetNative(): Boolean
    private external fun clearOutputResetNative()
    private external fun setLoopingNative(index: Int, looping: Boolean)
    private external fun startPlayingNative(index: Int, mode: OboePerformanceMode)
    private external fun stopPlayingNative(index: Int)
    private external fun updatePerformanceModeNative(mode: OboePerformanceMode)
    private external fun setMMapEnabledNative(enabled: Boolean): Boolean
    private external fun isMMapEnabledNative(): Boolean
    private external fun isMMapSupportedNative(): Boolean
    private external fun setBufferSizeInFramesNative(bufferSizeInFrames: Int): Int
    private external fun getBufferCapacityInFramesNative(): Int
    private external fun setVolumeNative(volume: Float)
    private external fun isOffloadedNative(): Boolean
    private external fun getCurrentlyPlayingIndexNative(): Int
    private external fun getPlaybackPositionMillisNative(): Long
    private external fun seekToNative(positionMillis: Int)
    private external fun getDurationMillisNative(index: Int): Long
    private external fun getWavFileInfoNative(wavBytes: ByteArray): IntArray
    private external fun removeSampleSourceNative(index: Int): Boolean

    /**
     * Companion
     */
    companion object {
        private const val TAG = "PowerPlayAudioPlayer"

        /**
         * The default number of channels in the player Stream. 2 for Stereo Playback.
         */
        const val NUM_PLAY_CHANNELS: Int = 2

        /**
         * Maximum file size for local WAV files (256MB).
         */
        const val MAX_FILE_SIZE_BYTES: Int = 256 * 1024 * 1024
    }
}

sealed interface PlayerState {
    object NoResultYet : PlayerState
    object Initialized : PlayerState
    object Playing : PlayerState
    object Stopped : PlayerState
    object Error : PlayerState
}
