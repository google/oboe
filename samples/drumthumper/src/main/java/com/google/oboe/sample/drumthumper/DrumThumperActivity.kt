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
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.util.*
import kotlin.concurrent.schedule


class DrumThumperActivity : AppCompatActivity(), TriggerPad.DrumPadTriggerListener {
    private val TAG = "DrumThumperActivity"

    private var mDrumPlayer = DrumPlayer()

    private val mPluginReceiver: BroadcastReceiver = PluginBroadcastReceiver()
    private var mInstallingPlugReceiver = false

    init {
        // Load the library containing the a native code including the JNI  functions
        System.loadLibrary("drumthumper")
    }

    // Receive a broadcast Intent when a headset is plugged in or unplugged.
    inner class PluginBroadcastReceiver : BroadcastReceiver() {
        private val TAG = "PluginBroadcastReceiver"
        override fun onReceive(context: Context, intent: Intent) {
            if (isInitialStickyBroadcast) {
                // ignore the broadcast that comes from registering this receiver
                return
            }

            var state = intent.getIntExtra("state", -1)

            var message =
                if (state == 0) {
                    "Headset Removed"
                } else {
                    "Headset Connected"
                };

            Toast.makeText(context, message, Toast.LENGTH_SHORT).show()

            if (mDrumPlayer.getOutputReset()) {
                // the (native) stream has been reset by the onErrorAfterClose() callback
                mDrumPlayer.clearOutputReset();
            } else {
                // give the (native) stream a chance to close it.
                val timer = Timer("stream restart timer", false);

                // schedule a single event
                timer.schedule(3000) {
                    if (!mDrumPlayer.getOutputReset()) {
                        Log.i(TAG, "==== sTimer Retry");
                        // still didn't get reset, so lets do it ourselves
                        mDrumPlayer.restartStream();
                    }
                }
            }
        }
    }

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
        registerReceiver(mPluginReceiver, filter)

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

        unregisterReceiver(mPluginReceiver)
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
