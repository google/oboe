/*
 * Copyright 2019 The Android Open Source Project
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
package com.google.oboe.sample.drumthumper

import android.content.res.AssetManager
import android.util.Log
import java.io.IOException

public class DrumPlayer {
    companion object {
        // Sample attributes
        val NUM_CHANNELS: Int = 1
        val SAMPLE_RATE: Int = 44100

        // Sample Buffer IDs
        val NUM_SAMPLES: Int = 6
        val BASSDRUM: Int = 0
        val SNAREDRUM: Int = 1
        val CRASHCYMBAL: Int = 2
        val RIDECYMBAL: Int = 3
        val HIHATOPEN: Int = 4
        val HIHATCLOSED: Int = 5

        // Logging Tag
        val TAG: String = "DrumPlayer"
    }

    fun init() {
        initNative(NUM_SAMPLES, NUM_CHANNELS, SAMPLE_RATE)
    }

    // asset-based samples
    fun loadWavAssets(assetMgr: AssetManager) {
        loadWavAsset(assetMgr, "KickDrum.wav", BASSDRUM)
        loadWavAsset(assetMgr, "SnareDrum.wav", SNAREDRUM)
        loadWavAsset(assetMgr, "CrashCymbal.wav", CRASHCYMBAL)
        loadWavAsset(assetMgr, "RideCymbal.wav", RIDECYMBAL)
        loadWavAsset(assetMgr, "HiHat_Open.wav", HIHATOPEN)
        loadWavAsset(assetMgr, "HiHat_Closed.wav", HIHATCLOSED)
    }

    fun loadWavAsset(assetMgr: AssetManager, assetName: String, index: Int) {
        try {
            val assetFD = assetMgr.openFd(assetName)
            val dataStream = assetFD.createInputStream();
            var dataLen = assetFD.getLength().toInt()
            var dataBytes: ByteArray = ByteArray(dataLen)
            dataStream.read(dataBytes, 0, dataLen)
            loadWavAssetNative(dataBytes, index)
        } catch (ex: IOException) {
            Log.i(TAG, "IOException" + ex)
        }
    }

    external fun loadWavAssetNative(wavBytes: ByteArray, index: Int)

    external fun trigger(drumIndex: Int)
    external fun initNative(numSampleBuffers: Int, numChannels: Int, sampleRate: Int)
}
