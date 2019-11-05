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

import android.Manifest;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class AnalyzerActivity extends TestInputActivity {

    private static final int MY_PERMISSIONS_REQUEST_EXTERNAL_STORAGE = 1001;

    protected static final String KEY_FILE_NAME = "file";
    protected static final String KEY_BUFFER_BURSTS = "buffer_bursts";

    AudioOutputTester mAudioOutTester;
    protected BufferSizeView mBufferSizeView;
    protected String mResultFileName;
    private String mTestResults;

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

    @NonNull
    protected String getCommonTestReport() {
        StringBuffer report = new StringBuffer();
        // Add some extra information for the remote tester.
        report.append("build.fingerprint = " + Build.FINGERPRINT + "\n");
        try {
            PackageInfo pinfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            report.append(String.format("test.version = %s\n", pinfo.versionName));
            report.append(String.format("test.version.code = %d\n", pinfo.versionCode));
        } catch (PackageManager.NameNotFoundException e) {
        }
        report.append("time.millis = " + System.currentTimeMillis() + "\n");

        // INPUT
        report.append(mAudioInputTester.actualConfiguration.dump());
        AudioStreamBase inStream = mAudioInputTester.getCurrentAudioStream();
        report.append(String.format("in.burst.frames = %d\n", inStream.getFramesPerBurst()));
        report.append(String.format("in.xruns = %d\n", inStream.getXRunCount()));

        // OUTPUT
        report.append(mAudioOutTester.actualConfiguration.dump());
        AudioStreamBase outStream = mAudioOutTester.getCurrentAudioStream();
        report.append(String.format("out.burst.frames = %d\n", outStream.getFramesPerBurst()));
        int bufferSize = outStream.getBufferSizeInFrames();
        report.append(String.format("out.buffer.size.frames = %d\n", bufferSize));
        int bufferCapacity = outStream.getBufferCapacityInFrames();
        report.append(String.format("out.buffer.capacity.frames = %d\n", bufferCapacity));
        report.append(String.format("out.xruns = %d\n", outStream.getXRunCount()));

        return report.toString();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mAudioOutTester = addAudioOutputTester();
        mBufferSizeView = (BufferSizeView) findViewById(R.id.buffer_size_view);
        if (mBufferSizeView != null) {
            mBufferSizeView.setAudioOutTester(mAudioOutTester);
        }
    }

    @Override
    protected void resetConfiguration() {
        super.resetConfiguration();
        mAudioOutTester.reset();

        StreamContext streamContext = getFirstInputStreamContext();
        if (streamContext != null) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.setFormat(StreamConfiguration.AUDIO_FORMAT_PCM_FLOAT);
                streamContext.configurationView.setFormatConversionAllowed(true);
            }
        }
        streamContext = getFirstOutputStreamContext();
        if (streamContext != null) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.setFormat(StreamConfiguration.AUDIO_FORMAT_PCM_FLOAT);
                streamContext.configurationView.setFormatConversionAllowed(true);
            }
        }
    }

    public void startAudio() {
        if (mBufferSizeView != null && mBufferSizeView.isEnabled()) {
            mBufferSizeView.updateBufferSize();
        }
        super.startAudio();
    }

    public void onStreamClosed() {
        Toast.makeText(getApplicationContext(),
                "Stream was closed or disconnected!",
                Toast.LENGTH_SHORT)
                .show();
        stopAudioTest();
    }

    public void stopAudioTest() {
    }

    void writeTestResultIfPermitted(String resultString) {
        // Here, thisActivity is the current activity
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            mTestResults = resultString;
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    MY_PERMISSIONS_REQUEST_EXTERNAL_STORAGE);
        } else {
            // Permission has already been granted
            writeTestResult(resultString);
        }
    }

    void maybeWriteTestResult(String resultString) {
        if (mResultFileName == null) return;
        writeTestResultIfPermitted(resultString);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {
        switch (requestCode) {
            case MY_PERMISSIONS_REQUEST_EXTERNAL_STORAGE: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    writeTestResult(mTestResults);
                } else {
                    showToast("Writing external storage needed for test results.");
                }
                return;
            }
        }
    }

    private void writeTestInBackground(final String resultString) {
        new Thread() {
            public void run() {
                writeTestResult(resultString);
            }
        }.start();
    }

    // Run this in a background thread.
    private void writeTestResult(String resultString) {
        File resultFile = new File(mResultFileName);
        Writer writer = null;
        try {
            writer = new OutputStreamWriter(new FileOutputStream(resultFile));
            writer.write(resultString);
        } catch (
                IOException e) {
            e.printStackTrace();
            showErrorToast(" writing result file. " + e.getMessage());
        } finally {
            if (writer != null) {
                try {
                    writer.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        mResultFileName = null;
    }


}
