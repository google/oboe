/*
 * Copyright 2022 The Android Open Source Project
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

package com.example.minimaloboe

class AudioPlayer {

    init {
        // Load the library containing the native code including the JNI functions.
        System.loadLibrary("minimaloboe")
    }

    fun startAudio(): Int {
        return startAudioStreamNative()
    }

    fun stopAudio(): Int {
        return stopAudioStreamNative()
    }


    private external fun startAudioStreamNative(): Int
    private external fun stopAudioStreamNative(): Int
}
