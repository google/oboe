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

import android.content.Context
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Bundle

import android.widget.Toast

import androidx.appcompat.app.AppCompatActivity

import java.util.*

import kotlin.concurrent.schedule

class DrumThumperActivity : AppCompatActivity(), TriggerPad.DrumPadTriggerListener {
    private val TAG = "DrumThumperActivity"

    private var mAudioMgr: AudioManager? = null

    private var mDrumPlayer = DrumPlayer()

    private var mDeviceListener: DeviceListener = DeviceListener()

    private val mUseDeviceChangeFallback = true
    private var mDevicesInitialized = false

    init {
        // Load the library containing the a native code including the JNI  functions
        System.loadLibrary("drumthumper")
    }

    /*:
     * This  implements a "fallback" mechanism for devices that do not correctly call
     * AudioStreamCallback::onErrorAfterClose(). It works by noticing that the connected
     * Devices have changed, and so the player needs to be restarted.
     *
     * Caveat: This callback also gets called when it is installed in the AudioManager and in
     * that case, there is as yet no device change so no need to restart the audio stream.
     */
    inner class DeviceListener: AudioDeviceCallback() {
        override fun onAudioDevicesAdded(addedDevices: Array<AudioDeviceInfo> ) {
            // Note: This will get called when the callback is installed.
            if (mDevicesInitialized) {
                // This is not the initial callback, so devices have changed
                Toast.makeText(applicationContext, "Added Device", Toast.LENGTH_LONG).show()
                resetOutput()
            }
            mDevicesInitialized = true
        }

        override fun onAudioDevicesRemoved(removedDevices: Array<AudioDeviceInfo> ) {
            Toast.makeText(applicationContext, "Removed Device", Toast.LENGTH_LONG).show()
            resetOutput()
        }

        fun resetOutput() {
            if (mDrumPlayer.getOutputReset()) {
                // the (native) stream has been reset by the onErrorAfterClose() callback
                mDrumPlayer.clearOutputReset()
            } else {
                // give the (native) stream a chance to close it.
                val timer = Timer("stream restart timer", false)
                // schedule a single event
                timer.schedule(3000) {
                    if (!mDrumPlayer.getOutputReset()) {
                        // still didn't get reset, so lets do it ourselves
                        mDrumPlayer.restartStream();
                    }
                }
            }

        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        mAudioMgr = getSystemService(Context.AUDIO_SERVICE) as AudioManager
    }

    override fun onStart() {
        super.onStart()

    }

    override fun onResume() {
        super.onResume()

        if (mUseDeviceChangeFallback) {
            mAudioMgr!!.registerAudioDeviceCallback(mDeviceListener, null)
        }

        // UI
        setContentView(R.layout.drumthumper_activity)

        // hookup the UI
        run {
            var pad: TriggerPad = findViewById(R.id.kickPad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.snarePad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.midTomPad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.lowTomPad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.hihatOpenPad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.hihatClosedPad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.ridePad)
            pad.addListener(this)
        }

        run {
            var pad: TriggerPad = findViewById(R.id.crashPad)
            pad.addListener(this)
        }

        mDrumPlayer.setupAudioStream()
        mDrumPlayer.loadWavAssets(getAssets())
    }

    override fun onPause() {
        super.onPause()

        mAudioMgr!!.unregisterAudioDeviceCallback(mDeviceListener)
    }

    override fun onStop() {
        mDrumPlayer.teardownAudioStream()
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
    }

    //
    // DrumPad.DrumPadTriggerListener
    //
    override fun triggerDown(pad: TriggerPad) {
        // trigger the sound based on the pad
        when (pad.id) {
            R.id.kickPad -> mDrumPlayer.trigger(DrumPlayer.BASSDRUM)
            R.id.snarePad -> mDrumPlayer.trigger(DrumPlayer.SNAREDRUM)
            R.id.midTomPad -> mDrumPlayer.trigger(DrumPlayer.MIDTOM)
            R.id.lowTomPad -> mDrumPlayer.trigger(DrumPlayer.LOWTOM)
            R.id.hihatOpenPad -> mDrumPlayer.trigger(DrumPlayer.HIHATOPEN)
            R.id.hihatClosedPad -> mDrumPlayer.trigger(DrumPlayer.HIHATCLOSED)
            R.id.ridePad -> mDrumPlayer.trigger(DrumPlayer.RIDECYMBAL)
            R.id.crashPad -> mDrumPlayer.trigger(DrumPlayer.CRASHCYMBAL)
        }
    }

    override fun triggerUp(pad: TriggerPad) {
        // NOP
    }
}
