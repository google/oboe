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

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AppCompatActivity


class DrumThumperActivity : AppCompatActivity(), TriggerPad.DrumPadTriggerListener {
    private val TAG = "DrumThumperActivity"

    private var mDrumPlayer = DrumPlayer()

    init {
        // Load the library containing the a native code including the JNI  functions
        System.loadLibrary("drumthumper")
    }

    // Receive a broadcast Intent when a headset is plugged in or unplugged.
    public class PluginBroadcastReceiver : BroadcastReceiver() {
        private val TAG = "PluginBroadcastReceiver"
        override fun onReceive(context: Context?, intent: Intent?) {
            // Close the stream if it was not disconnected.
            Log.i(TAG, "")
        }
    }

    private val mPluginReceiver: BroadcastReceiver = PluginBroadcastReceiver()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun onStart() {
        super.onStart()
    }

    override fun onResume() {
        super.onResume()

        // Connect/Disconnect workaround
        val filter = IntentFilter(Intent.ACTION_HEADSET_PLUG)
        this.registerReceiver(mPluginReceiver, filter)

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
        unregisterReceiver(mPluginReceiver)

        mDrumPlayer.teardownAudioStream()
        super.onPause()

    }

    override fun onStop() {
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
