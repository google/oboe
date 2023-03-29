/*
 * Copyright 2023 The Android Open Source Project
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

package com.mobileer.oboetester;

import static com.mobileer.oboetester.TestAudioActivity.TAG;

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import java.util.Random;

/**
 * Try to crash in the native AAudio code by causing a routing change
 * while playing audio. The buffer may get deleted while we are writing to it!
 * See b/274815060
 */
public class TestRouteDuringCallbackActivity extends Activity {

    private TextView mStatusView;
    private MyStreamSniffer mStreamSniffer;
    private AudioManager mAudioManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_routing_crash);
        mStatusView = (TextView) findViewById(R.id.text_callback_status);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    }

    public void onTestRoutingCrash(View view) {
        stopSniffer();
        mStreamSniffer = new MyStreamSniffer();
        mStreamSniffer.start();
    }

    // Change routing while the stream is playing.
    // Keep trying until we crash.
    protected class MyStreamSniffer extends Thread {
        boolean enabled = true;
        int routingOption = 0;
        StringBuffer statusBuffer = new StringBuffer();

        @Override
        public void run() {
            routingOption = 0;
            changeRoute(routingOption);
            int result;
            Random random = new Random();
            while (enabled) {
                if (routingOption == 0) {
                    statusBuffer = new StringBuffer();
                }
                try {
                    sleep(100);
                    result = startStream();
                    sleep(100);
                    log("-------\nstartStream() returned " + result);
                    int sleepTimeMillis = 500 + random.nextInt(500);
                    sleep(sleepTimeMillis);
                    routingOption = (routingOption == 0) ? 1 : 0;
                    log("changeRoute " + routingOption);
                    changeRoute(routingOption);
                    sleep(50);
                } catch (InterruptedException e) {
                } finally {
                    result = stopStream();
                    log("stopStream() returned " + result);
                }
            }
            changeRoute(0);
        }

        // Log to screen and logcat.
        private void log(String text) {
            Log.e(TAG, "RoutingCrash: " + text);
            statusBuffer.append(text + ", sleep " + getSleepTimeMicros() + " us\n");
            showStatus(statusBuffer.toString());
        }

        // Stop the test thread.
        void finish() {
            enabled = false;
            interrupt();
            try {
                join(2000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    protected void showStatus(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStatusView.setText(message);
            }
        });
    }

    private void changeRoute(int option) {
        mAudioManager.setSpeakerphoneOn(option > 0);
    }

    private native int startStream();
    private native int getSleepTimeMicros();
    private native int stopStream();

    @Override
    public void onPause() {
        super.onPause();
        stopSniffer();
    }

    private void stopSniffer() {
        if (mStreamSniffer != null) {
            mStreamSniffer.finish();
            mStreamSniffer = null;
        }
    }
}
