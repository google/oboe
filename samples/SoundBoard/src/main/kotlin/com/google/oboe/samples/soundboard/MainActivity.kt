/*
 * Copyright 2021 The Android Open Source Project
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

package com.google.oboe.samples.soundboard

import android.media.AudioManager
import android.os.Bundle
import android.util.DisplayMetrics
import android.app.Activity
import android.content.Context
import android.graphics.Rect
import androidx.appcompat.app.AppCompatActivity
import java.util.ArrayList
import kotlin.math.min

class MainActivity : AppCompatActivity() {
    private external fun startEngine(numSignals: Int): Long
    private external fun stopEngine(engineHandle: Long)
    private external fun native_setDefaultStreamValues(sampleRate: Int, framesPerBurst: Int)

    companion object {
        private const val NUM_ROWS = 6
        private const val NUM_COLUMNS = 5
        private var mEngineHandle: Long = 0

        // Used to load the 'native-lib' library on application startup.
        init {
            System.loadLibrary("soundboard")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }

    override fun onResume() {
        setDefaultStreamValues(this)
        mEngineHandle = startEngine(NUM_ROWS * NUM_COLUMNS)
        createMusicTiles(this)
        super.onResume()
    }

    override fun onPause() {
        stopEngine(mEngineHandle)
        super.onPause()
    }

    private fun setDefaultStreamValues(context: Context) {
        val myAudioMgr = context.getSystemService(AUDIO_SERVICE) as AudioManager
        val sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
        val defaultSampleRate = sampleRateStr.toInt()
        val framesPerBurstStr =
            myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
        val defaultFramesPerBurst = framesPerBurstStr.toInt()
        native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst)
    }

    private fun createMusicTiles(context: Context) {
        val displayMetrics = DisplayMetrics()
        (context as Activity).windowManager.defaultDisplay.getMetrics(displayMetrics)
        val height = displayMetrics.heightPixels
        val width = displayMetrics.widthPixels

        // 5 by 6 tiles
        val numRows = NUM_ROWS
        val numColumns = NUM_COLUMNS
        val tileLength = min(height / numRows, width / numColumns)
        val xStartLocation = (width - tileLength * numColumns) / 2
        // Height isn't a perfect measurement so shift the location slightly up from the "center"
        val yStartLocation = (height - tileLength * numRows) / 2 / 2
        val rectangles = ArrayList<Rect>()
        for (i in 0 until numRows) {
            for (j in 0 until numColumns) {
                val rectangle = Rect(
                    xStartLocation + j * tileLength,
                    yStartLocation + i * tileLength,
                    xStartLocation + j * tileLength + tileLength,
                    yStartLocation + i * tileLength + tileLength
                )
                rectangles.add(rectangle)
            }
        }
        setContentView(MusicTileView(this, rectangles, NoteListener(mEngineHandle)))
    }
}
