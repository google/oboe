/*
 * Copyright 2015 The Android Open Source Project
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
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import com.mobileer.miditools.MidiOutputPortConnectionSelector;
import com.mobileer.miditools.MidiPortConnector;
import com.mobileer.miditools.MidiTools;

import java.io.IOException;

import static com.google.sample.oboe.manualtest.AudioMidiTester.TestListener;
import static com.google.sample.oboe.manualtest.AudioMidiTester.TestResult;

public class TapToToneActivity extends TestOutputActivityBase {
    private static final int MY_PERMISSIONS_REQUEST_RECORD_AUDIO = 1234;
    private TextView mResultView;
    private MidiManager mMidiManager;
    private MidiInputPort mInputPort;

    protected AudioMidiTester mAudioMidiTester;

    private MidiOutputPortConnectionSelector mPortSelector;
    private MyTestListener mTestListener = new MyTestListener();
    private WaveformView mWaveformView;
    // Stats for latency
    private int mMeasurementCount;
    private int mLatencySumSamples;
    private int mLatencyMin;
    private int mLatencyMax;

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_tap_to_tone);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAudioOutTester = addAudioOutputTester();

        mResultView = (TextView) findViewById(R.id.resultView);

        if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            setupMidi();
        } else {
            Toast.makeText(TapToToneActivity.this,
                    "MIDI not supported!", Toast.LENGTH_LONG)
                    .show();
        }

        mWaveformView = (WaveformView) findViewById(R.id.waveview_audio);

        // Start a blip test when the waveform view is tapped.
        mWaveformView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent event) {
                int action = event.getActionMasked();
                switch (action) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_POINTER_DOWN:
                        mAudioMidiTester.setEnabled(true);
                        break;
                    case MotionEvent.ACTION_MOVE:
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_POINTER_UP:
                        mAudioMidiTester.setEnabled(false);
                        break;
                }
                // Must return true or we do not get the ACTION_MOVE and
                // ACTION_UP events.
                return true;
            }
        });

        updateEnabledWidgets();
    }

    @Override
    protected void onStart() {
        super.onStart();
        setActivityType(ACTIVITY_TAP_TO_TONE);
    }

    @Override
    protected void onDestroy() {
        mAudioMidiTester.removeTestListener(mTestListener);
        closeMidiResources();
        super.onDestroy();
    }

    private void setupMidi() {
        // Setup MIDI
        mMidiManager = (MidiManager) getSystemService(MIDI_SERVICE);
        MidiDeviceInfo[] infos = mMidiManager.getDevices();

        // Open the port now so that the AudioMidiTester gets created.
        for (MidiDeviceInfo info : infos) {
            Bundle properties = info.getProperties();
            String product = properties
                    .getString(MidiDeviceInfo.PROPERTY_PRODUCT);

            Log.i(TAG, "product = " + product);
            if ("AudioLatencyTester".equals(product)) {
                openPort(info);
                break;
            }
        }

    }

    // These should only be set after mAudioMidiTester is set.
    private void setSpinnerListeners() {
        MidiDeviceInfo synthInfo = MidiTools.findDevice(mMidiManager, "AndroidTest",
                "AudioLatencyTester");
        Log.i(TAG, "found tester virtual device info: " + synthInfo);
        int portIndex = 0;
        mPortSelector = new MidiOutputPortConnectionSelector(mMidiManager, this,
                R.id.spinner_synth_sender, synthInfo, portIndex);
        mPortSelector.setConnectedListener(new MyPortsConnectedListener());

    }

    private class MyTestListener implements TestListener {
        @Override
        public void onTestFinished(final TestResult result) {
            runOnUiThread(new Runnable() {
                public void run() {
                    showTestResults(result);
                }
            });
        }

        @Override
        public void onNoteOn(final int pitch) {
            runOnUiThread(new Runnable() {
                public void run() {
                    mStreamContexts.get(0).configurationView.setStatusText("MIDI pitch = " + pitch);
                }
            });
        }
    }

    // Runs on UI thread.
    private void showTestResults(TestResult result) {
        String text;
        int previous = 0;
        if (result == null) {
            text = "";
            mWaveformView.clearSampleData();
        } else {
            if (result.events.length < 2) {
                text = "Not enough edges. Use fingernail.\n";
                mWaveformView.setCursorData(null);
            } else if (result.events.length > 2) {
                text = "Too many edges.\n";
                mWaveformView.setCursorData(null);
            } else {
                int[] cursors = new int[2];
                cursors[0] = result.events[0].sampleIndex;
                cursors[1] = result.events[1].sampleIndex;
                int latencySamples = cursors[1] - cursors[0];
                mLatencySumSamples += latencySamples;
                mMeasurementCount++;

                int latencyMillis = 1000 * latencySamples / result.frameRate;
                if (mLatencyMin > latencyMillis) {
                    mLatencyMin = latencyMillis;
                }
                if (mLatencyMax < latencyMillis) {
                    mLatencyMax = latencyMillis;
                }

                text = String.format("latency = %3d msec\n", latencyMillis);
                mWaveformView.setCursorData(cursors);
            }
            mWaveformView.setSampleData(result.filtered);
        }

        if (mMeasurementCount > 0) {
            int averageLatencySamples = mLatencySumSamples / mMeasurementCount;
            int averageLatencyMillis = 1000 * averageLatencySamples / result.frameRate;
            final String plural = (mMeasurementCount == 1) ? "test" : "tests";
            text = text + String.format("min = %3d, avg = %3d, max = %3d, %d %s",
                    mLatencyMin, averageLatencyMillis, mLatencyMax, mMeasurementCount, plural);
        }
        final String postText = text;
        mWaveformView.post(new Runnable() {
            public void run() {
                mResultView.setText(postText);
                mWaveformView.postInvalidate();
            }
        });
    }

    private void openPort(final MidiDeviceInfo info) {
        mMidiManager.openDevice(info, new MidiManager.OnDeviceOpenedListener() {
                    @Override
                    public void onDeviceOpened(MidiDevice device) {
                        if (device == null) {
                            Log.e(TAG, "could not open device " + info);
                        } else {
                            mInputPort = device.openInputPort(0);
                            Log.i(TAG, "opened MIDI port = " + mInputPort + " on " + info);
                            mAudioMidiTester = AudioMidiTester.getInstance();

                            Log.i(TAG, "openPort() mAudioMidiTester = " + mAudioMidiTester);
                            // Now that we have created the AudioMidiTester, close the port so we can
                            // open it later.
                            try {
                                mInputPort.close();
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                            mAudioMidiTester.addTestListener(mTestListener);

                            setSpinnerListeners();
                        }
                    }
                }, new Handler(Looper.getMainLooper())
        );
    }

    // TODO Listen to the synth server
    // for open/close events and then disable/enable the spinner.
    private class MyPortsConnectedListener
            implements MidiPortConnector.OnPortsConnectedListener {
        @Override
        public void onPortsConnected(final MidiDevice.MidiConnection connection) {
            Log.i(TAG, "onPortsConnected, connection = " + connection);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (connection == null) {
                        Toast.makeText(TapToToneActivity.this,
                                R.string.error_port_busy, Toast.LENGTH_LONG)
                                .show();
                        mPortSelector.clearSelection();
                    } else {
                        Toast.makeText(TapToToneActivity.this,
                                R.string.port_open_ok, Toast.LENGTH_LONG)
                                .show();
                    }
                }
            });
        }
    }

    private void closeMidiResources() {
        if (mPortSelector != null) {
            mPortSelector.close();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private boolean hasRecordAudioPermission(){
        boolean hasPermission = (checkSelfPermission(
                Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED);
        Log.i(TAG, "Has RECORD_AUDIO permission? " + hasPermission);
        return hasPermission;
    }

    private void requestRecordAudioPermission(){

        String requiredPermission = Manifest.permission.RECORD_AUDIO;

        // If the user previously denied this permission then show a message explaining why
        // this permission is needed
        if (shouldShowRequestPermissionRationale(requiredPermission)) {
            showErrorToast("This app needs to record audio through the microphone....");
        }

        // request the permission.
        requestPermissions(new String[]{requiredPermission},
                MY_PERMISSIONS_REQUEST_RECORD_AUDIO);
    }
    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        // TODO
    }

    @Override
    public void startAudio() {
        if (hasRecordAudioPermission()) {
            startAudioPermitted();
        } else {
            requestRecordAudioPermission();
        }
    }

    private void startAudioPermitted() {
        super.startAudio();
        resetLatency();
        try {
            mAudioMidiTester.start();
            if (mAudioOutTester != null) {
                mAudioOutTester.setToneType(OboeAudioOutputStream.TONE_TYPE_SAW_PING);
            } else {
                Log.w(TAG, "startAudioPermitted, mAudioOutTester = null, cannot setToneType(ping)");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void stopAudio() {
        mAudioMidiTester.stop();
        super.stopAudio();
    }

    @Override
    public void pauseAudio() {
        mAudioMidiTester.stop();
        super.pauseAudio();
    }

    @Override
    public void closeAudio() {
        mAudioMidiTester.stop();
        super.closeAudio();
    }

    private void resetLatency() {
        mMeasurementCount = 0;
        mLatencySumSamples = 0;
        mLatencyMin = Integer.MAX_VALUE;
        mLatencyMax = 0;
        showTestResults(null);
    }

}
