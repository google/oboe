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

import android.content.res.AssetManager
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
    fun setupAudioStream() {
        setupAudioStreamNative(NUM_PLAY_CHANNELS)
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
     * Loads the file into memory
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

    /**
     * Companion
     */
    companion object {
        /**
         * The number of channels in the player Stream. 2 for Stereo Playback, set to 1 for Mono playback.
         */
        const val NUM_PLAY_CHANNELS: Int = 2
    }
}

sealed interface PlayerState {
    object NoResultYet : PlayerState
    object Initialized : PlayerState
    object Playing : PlayerState
    object Stopped : PlayerState
    object Error : PlayerState
}
