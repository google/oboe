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

/**
 * Represents the audio properties of a parsed WAV file.
 *
 * @param sampleRate The sample rate in Hz (e.g., 44100, 48000, 96000)
 * @param numChannels The number of audio channels (1=mono, 2=stereo)
 * @param bitsPerSample The bit depth per sample (8, 16, 24, or 32)
 * @param encoding The encoding format (1=PCM, 3=IEEE Float)
 * @param numSampleFrames The total number of sample frames in the file
 * @param durationMs The total duration in milliseconds
 */
data class WavFileInfo(
    val sampleRate: Int,
    val numChannels: Int,
    val bitsPerSample: Int,
    val encoding: Int,
    val numSampleFrames: Long,
    val durationMs: Long
) {
    val encodingName: String
        get() = when (encoding) {
            ENCODING_PCM -> "PCM"
            ENCODING_IEEE_FLOAT -> "IEEE Float"
            else -> "Unknown"
        }

    val channelLayoutName: String
        get() = when (numChannels) {
            1 -> "Mono"
            2 -> "Stereo"
            else -> "$numChannels channels"
        }

    companion object {
        const val ENCODING_PCM = 1
        const val ENCODING_IEEE_FLOAT = 3
    }
}
