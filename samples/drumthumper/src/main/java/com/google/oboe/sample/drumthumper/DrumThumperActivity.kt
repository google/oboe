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

class DrumThumperActivity : AppCompatActivity() {
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
        run {
            var btn: Button = findViewById(R.id.kickBtn)
            btn.setOnClickListener {
                mDrumPlayer.trigger(DrumPlayer.BASSDRUM)
            }
        }

        run {
            var btn: Button = findViewById(R.id.snareBtn)
            btn.setOnClickListener {
                mDrumPlayer.trigger(DrumPlayer.SNAREDRUM)
            }
        }

        run {
            var btn: Button = findViewById(R.id.crashBtn)
            btn.setOnClickListener {
                mDrumPlayer.trigger(DrumPlayer.CRASHCYMBAL)
            }
        }

        run {
            var btn: Button = findViewById(R.id.hihatOpenBtn)
            btn.setOnClickListener {
                mDrumPlayer.trigger(DrumPlayer.HIHATOPEN)
            }
        }

        run {
            var btn: Button = findViewById(R.id.hihatClosedBtn)
            btn.setOnClickListener {
                mDrumPlayer.trigger(DrumPlayer.HIHATCLOSED)
            }
        }
    }
}
