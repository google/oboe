/*
 * Copyright 2018 The Android Open Source Project
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

package com.google.sample.oboe.manualtest;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class AnalyzerActivity extends TestInputActivity {

    AudioOutputTester mAudioOutTester;

    // Note that these string must match the enum result_code in LatencyAnalyzer.h
    String resultCodeToString(int resultCode) {
        switch (resultCode) {
            case 0:
                return "OK";
            case -99:
                return "ERROR_NOISY";
            case -98:
                return "ERROR_VOLUME_TOO_LOW";
            case -97:
                return "ERROR_VOLUME_TOO_HIGH";
            case -96:
                return "ERROR_CONFIDENCE";
            case -95:
                return "ERROR_INVALID_STATE";
            case -94:
                return "ERROR_GLITCHES";
            case -93:
                return "ERROR_NO_LOCK";
            default:
                return "UNKNOWN";
        }
    }

    public native int getAnalyzerState();
    public native boolean isAnalyzerDone();
    public native int getMeasuredResult();
    public native int getResetCount();

    public void onStreamClosed() {
        Toast.makeText(getApplicationContext(),
                "Stream was closed or disconnected!",
                Toast.LENGTH_SHORT)
                .show();
        stopAudioTest();
    }

    public void stopAudioTest() {
    }
}
