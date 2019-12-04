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

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button

class DrumThumperActivity : AppCompatActivity(), DrumPad.DrumPadTriggerListener {
    private var mDrumPlayer = DrumPlayer()

    init {
        System.loadLibrary("drumthumper")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        mDrumPlayer.init();
        mDrumPlayer.loadWavAssets(getAssets())

        // setup button handlers
//        run {
//            var btn: Button = findViewById(R.id.kickBtn)
//            btn.setOnClickListener {
//                mDrumPlayer.triggerDown(DrumPlayer.BASSDRUM)
//            }
//        }
//
//        run {
//            var btn: Button = findViewById(R.id.snareBtn)
//            btn.setOnClickListener {
//                mDrumPlayer.triggerDown(DrumPlayer.SNAREDRUM)
//            }
//        }

//        run {
//            var btn: Button = findViewById(R.id.crashBtn)
//            btn.setOnClickListener {
//                mDrumPlayer.triggerDown(DrumPlayer.CRASHCYMBAL)
//            }
//        }

//        run {
//            var btn: Button = findViewById(R.id.hihatOpenBtn)
//            btn.setOnClickListener {
//                mDrumPlayer.triggerDown(DrumPlayer.HIHATOPEN)
//            }
//        }
//
//        run {
//            var btn: Button = findViewById(R.id.hihatClosedBtn)
//            btn.setOnClickListener {
//                mDrumPlayer.triggerDown(DrumPlayer.HIHATCLOSED)
//            }
//        }

        run {
            var pad: DrumPad = findViewById(R.id.kickPad)
            pad.addListener(this)
        }

        run {
            var pad: DrumPad = findViewById(R.id.snarePad)
            pad.addListener(this)
        }

        run {
            var pad: DrumPad = findViewById(R.id.hihatOpenPad)
            pad.addListener(this)
        }

        run {
            var pad: DrumPad = findViewById(R.id.hihatClosedPad)
            pad.addListener(this)
        }

        run {
            var pad: DrumPad = findViewById(R.id.ridePad)
            pad.addListener(this)
        }

        run {
            var pad: DrumPad = findViewById(R.id.crashPad)
            pad.addListener(this)
        }
    }

    //
    // DrumPad.DrumPadTriggerListener
    //
    override fun trigger(pad: DrumPad) {
        when (pad.id) {
            R.id.kickPad -> mDrumPlayer.trigger(DrumPlayer.BASSDRUM)
            R.id.snarePad -> mDrumPlayer.trigger(DrumPlayer.SNAREDRUM)
            R.id.hihatOpenPad -> mDrumPlayer.trigger(DrumPlayer.HIHATOPEN)
            R.id.hihatClosedPad -> mDrumPlayer.trigger(DrumPlayer.HIHATCLOSED)
            R.id.ridePad -> mDrumPlayer.trigger(DrumPlayer.RIDECYMBAL)
            R.id.crashPad -> mDrumPlayer.trigger(DrumPlayer.CRASHCYMBAL)
        }
    }

}
