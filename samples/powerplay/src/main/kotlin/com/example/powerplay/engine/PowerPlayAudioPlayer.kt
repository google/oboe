package com.example.powerplay.engine

import android.content.res.AssetManager
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.asLiveData
import com.example.powerplay.MainActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus

class PowerPlayAudioPlayer() : DefaultLifecycleObserver {
    /**
     *
     */
    private var _playerState = MutableStateFlow<PlayerState>(PlayerState.NoResultYet)
    fun getPlayerStateLive() = _playerState.asLiveData()

    /**
     * Native passthrough functions
     */
    fun setupAudioStream() {
        setupAudioStreamNative(NUM_PLAY_CHANNELS)

        // TODO - Handle real response from native code
        _playerState.update { PlayerState.Initialized }
    }

    fun startPlaying(index: Int, offload: Int) {
        startPlayingNative(index, offload)

        // TODO - Handle real response from native code
        _playerState.update { PlayerState.Playing }
    }

    fun stopPlaying(index: Int) {
        stopPlayingNative(index)

        // TODO - Handle real response from native code
        _playerState.update { PlayerState.Stopped }
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
    private external fun startPlayingNative(index: Int, mode: Int)
    private external fun stopPlayingNative(index: Int)

    /**
     * Companion
     */
    companion object {
        /**
         * Logging Tag
         */
        const val TAG: String = "PowerPlayAudioEngine"

        /**
         * The number of channels in the player Stream. 2 for Stereo Playback, set to 1 for Mono playback.
         */
        const val NUM_PLAY_CHANNELS: Int = 2
    }
}

sealed interface PlayerState {
    object NoResultYet : PlayerState
    object Initialized : PlayerState
    object StreamStarted : PlayerState
    object Playing : PlayerState
    object Stopped : PlayerState
    data class Unknown(val resultCode: Int) : PlayerState
}
