/*
 * Copyright 2017 The Android Open Source Project
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
package com.google.sample.oboe.hellooboe

import android.app.Activity
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.ArrayAdapter
import android.widget.SimpleAdapter
import android.widget.Spinner
import android.widget.TextView
import com.google.sample.audio_device.AudioDeviceListEntry
import com.google.sample.audio_device.AudioDeviceSpinner
import com.google.sample.oboe.hellooboe.PlaybackEngine.create
import com.google.sample.oboe.hellooboe.PlaybackEngine.currentOutputLatencyMillis
import com.google.sample.oboe.hellooboe.PlaybackEngine.delete
import com.google.sample.oboe.hellooboe.PlaybackEngine.isLatencyDetectionSupported
import com.google.sample.oboe.hellooboe.PlaybackEngine.setAudioApi
import com.google.sample.oboe.hellooboe.PlaybackEngine.setAudioDeviceId
import com.google.sample.oboe.hellooboe.PlaybackEngine.setBufferSizeInBursts
import com.google.sample.oboe.hellooboe.PlaybackEngine.setChannelCount
import com.google.sample.oboe.hellooboe.PlaybackEngine.setToneOn
import java.util.ArrayList
import java.util.HashMap
import java.util.Locale
import java.util.Timer
import java.util.TimerTask

class MainActivity : Activity() {
    private var mAudioApiSpinner: Spinner? = null
    private var mPlaybackDeviceSpinner: AudioDeviceSpinner? = null
    private var mChannelCountSpinner: Spinner? = null
    private var mBufferSizeSpinner: Spinner? = null
    private var mLatencyText: TextView? = null
    private var mLatencyUpdater: Timer? = null

    /*
     * Hook to user control to start / stop audio playback:
     *    touch-down: start, and keeps on playing
     *    touch-up: stop.
     * simply pass the events to native side.
     */
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        when (action) {
            MotionEvent.ACTION_DOWN -> setToneOn(true)
            MotionEvent.ACTION_UP -> setToneOn(false)
        }
        return super.onTouchEvent(event)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        mLatencyText = findViewById(R.id.latencyText)
        setupAudioApiSpinner()
        setupPlaybackDeviceSpinner()
        setupChannelCountSpinner()
        setupBufferSizeSpinner()
    }

    /*
    * Creating engine in onResume() and destroying in onPause() so the stream retains exclusive
    * mode only while in focus. This allows other apps to reclaim exclusive stream mode.
    */
    override fun onResume() {
        super.onResume()
        create(this)
        setupLatencyUpdater()
        // Return the spinner states to their default value
        mChannelCountSpinner?.setSelection(CHANNEL_COUNT_DEFAULT_OPTION_INDEX)
        mPlaybackDeviceSpinner?.setSelection(SPINNER_DEFAULT_OPTION_INDEX)
        mBufferSizeSpinner?.setSelection(SPINNER_DEFAULT_OPTION_INDEX)
        mAudioApiSpinner?.setSelection(SPINNER_DEFAULT_OPTION_INDEX)
    }

    override fun onPause() {
        if (mLatencyUpdater != null) mLatencyUpdater?.cancel()
        delete()
        super.onPause()
    }

    private fun setupChannelCountSpinner() {
        mChannelCountSpinner = findViewById(R.id.channelCountSpinner)
        val channelCountAdapter =
            ArrayAdapter(this, R.layout.channel_counts_spinner, CHANNEL_COUNT_OPTIONS)
        mChannelCountSpinner?.apply {
            adapter = channelCountAdapter
            setSelection(CHANNEL_COUNT_DEFAULT_OPTION_INDEX)
            onItemSelectedListener = object : OnItemSelectedListener {
                override fun onNothingSelected(parent: AdapterView<*>?) {
                    // Do nothing
                }

                override fun onItemSelected(adapterView: AdapterView<*>?, view: View, i: Int, l: Long) {
                    setChannelCount(
                        CHANNEL_COUNT_OPTIONS[selectedItemPosition]
                    )
                }
            }
        }
    }

    private fun setupBufferSizeSpinner() {
        mBufferSizeSpinner = findViewById(R.id.bufferSizeSpinner)
        mBufferSizeSpinner?.apply {
            adapter = SimpleAdapter(
                context,
                createBufferSizeOptionsList(),  // list of buffer size options
                R.layout.buffer_sizes_spinner, arrayOf(getString(R.string.buffer_size_description_key)), intArrayOf(R.id.bufferSizeOption)
            )

            onItemSelectedListener = object : OnItemSelectedListener {
                override fun onItemSelected(adapterView: AdapterView<*>?, view: View, i: Int, l: Long) {
                    setBufferSizeInBursts(bufferSizeInBursts)
                }

                override fun onNothingSelected(parent: AdapterView<*>?) {
                    // Do nothing
                }
            }
        }
    }

    private fun setupPlaybackDeviceSpinner() {
        mPlaybackDeviceSpinner = findViewById(R.id.playbackDevicesSpinner)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mPlaybackDeviceSpinner?.apply {
                setDirectionType(AudioManager.GET_DEVICES_OUTPUTS)
                onItemSelectedListener = object : OnItemSelectedListener {
                    override fun onItemSelected(adapterView: AdapterView<*>?, view: View, i: Int, l: Long) {
                        setAudioDeviceId(playbackDeviceId)
                    }

                    override fun onNothingSelected(parent: AdapterView<*>?) {
                        // Do nothing
                    }
                }
            }
        }
    }

    private fun setupAudioApiSpinner() {
        mAudioApiSpinner = findViewById(R.id.audioApiSpinner)
        mAudioApiSpinner?.apply {
            adapter = SimpleAdapter(
                context,
                createAudioApisOptionsList(),
                R.layout.audio_apis_spinner, arrayOf(getString(R.string.audio_api_description_key)), intArrayOf(R.id.audioApiOption)
            )
            onItemSelectedListener = object : OnItemSelectedListener {
                override fun onItemSelected(adapterView: AdapterView<*>?, view: View, i: Int, l: Long) {
                    setAudioApi(i)
                }

                override fun onNothingSelected(parent: AdapterView<*>?) {
                    // Do nothing
                }
            }
        }
    }

    private val playbackDeviceId: Int
        get() = (mPlaybackDeviceSpinner?.selectedItem as AudioDeviceListEntry).id

    // parseInt will throw a NumberFormatException if the string doesn't contain a valid integer
    // representation. We don't need to worry about this because the values are derived from
    // the BUFFER_SIZE_OPTIONS int array.
    private val bufferSizeInBursts: Int
        get() {
            val selectedOption = mBufferSizeSpinner?.selectedItem as HashMap<String, String>
            val valueKey = getString(R.string.buffer_size_value_key)

            // parseInt will throw a NumberFormatException if the string doesn't contain a valid integer
            // representation. We don't need to worry about this because the values are derived from
            // the BUFFER_SIZE_OPTIONS int array.
            return selectedOption[valueKey]?.toInt() ?: 0
        }

    private fun setupLatencyUpdater() {
        //Update the latency every 1s
        val latencyUpdateTask: TimerTask = object : TimerTask() {
            override fun run() {
                val latencyStr: String
                latencyStr = if (isLatencyDetectionSupported) {
                    val latency = currentOutputLatencyMillis
                    if (latency >= 0) {
                        String.format(Locale.getDefault(), "%.2fms", latency)
                    } else {
                        "Unknown"
                    }
                } else {
                    getString(R.string.only_supported_on_api_26)
                }
                runOnUiThread { mLatencyText?.text = getString(R.string.latency, latencyStr) }
            }
        }
        mLatencyUpdater = Timer()
        mLatencyUpdater?.schedule(latencyUpdateTask, 0, UPDATE_LATENCY_EVERY_MILLIS)
    }

    /**
     * Creates a list of buffer size options which can be used to populate a SimpleAdapter.
     * Each option has a description and a value. The description is always equal to the value,
     * except when the value is zero as this indicates that the buffer size should be set
     * automatically by the audio engine
     *
     * @return list of buffer size options
     */
    private fun createBufferSizeOptionsList(): List<HashMap<String, String?>> {
        val bufferSizeOptions = ArrayList<HashMap<String, String?>>()
        for (i in BUFFER_SIZE_OPTIONS) {
            val option = HashMap<String, String?>()
            val strValue = i.toString()
            val description = if (i == 0) getString(R.string.automatic) else strValue
            option[getString(R.string.buffer_size_description_key)] = description
            option[getString(R.string.buffer_size_value_key)] = strValue
            bufferSizeOptions.add(option)
        }
        return bufferSizeOptions
    }

    private fun createAudioApisOptionsList(): List<HashMap<String, String?>> {
        val audioApiOptions = ArrayList<HashMap<String, String?>>()
        for (i in AUDIO_API_OPTIONS.indices) {
            val option = HashMap<String, String?>()
            option[getString(R.string.buffer_size_description_key)] = AUDIO_API_OPTIONS[i]
            option[getString(R.string.buffer_size_value_key)] = i.toString()
            audioApiOptions.add(option)
        }
        return audioApiOptions
    }

    companion object {
        private const val UPDATE_LATENCY_EVERY_MILLIS: Long = 1000
        private val CHANNEL_COUNT_OPTIONS = arrayOf(1, 2, 3, 4, 5, 6, 7, 8)

        // Default to Stereo (OPTIONS is zero-based array so index 1 = 2 channels)
        private const val CHANNEL_COUNT_DEFAULT_OPTION_INDEX = 1
        private val BUFFER_SIZE_OPTIONS = intArrayOf(0, 1, 2, 4, 8)
        private val AUDIO_API_OPTIONS = arrayOf("Unspecified", "OpenSL ES", "AAudio")

        // Default all other spinners to the first option on the list
        private const val SPINNER_DEFAULT_OPTION_INDEX = 0
    }
}