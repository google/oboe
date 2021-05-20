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

package com.mobileer.oboetester;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;

abstract class NativeSniffer implements Runnable {
    public static final int SNIFFER_UPDATE_PERIOD_MSEC = 100;
    public static final int SNIFFER_UPDATE_DELAY_MSEC = 200;
    private final Activity activity;
    protected Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread
    protected volatile boolean mEnabled = true;

    public NativeSniffer(Activity activity) {
        this.activity = activity;
    }

    public void startSniffer() {
        long now = System.currentTimeMillis();
        // Start the initial runnable task by posting through the handler
        mEnabled = true;
        mHandler.postDelayed(this, SNIFFER_UPDATE_DELAY_MSEC);
    }

    public void stopSniffer() {
        mEnabled = false;
        if (mHandler != null) {
            mHandler.removeCallbacks(this);
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateStatusText();
            }
        });
    }

    public void reschedule() {
        updateStatusText();
        // Reschedule so this task repeats
        if (mEnabled) {
            mHandler.postDelayed(this, SNIFFER_UPDATE_PERIOD_MSEC);
        }
    }

    public abstract void updateStatusText();

    public String getShortReport() {
        return "no-report";
    }

}
