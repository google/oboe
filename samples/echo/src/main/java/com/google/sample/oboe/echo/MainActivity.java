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

package com.google.sample.oboe.echo;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import com.google.sample.audio_device.AudioDeviceListEntry;
import com.google.sample.audio_device.AudioDeviceSpinner;

import java.io.IOException;

/**
 * TODO: Update README.md and go through and comment sample
 */
public class MainActivity extends Activity
        implements ActivityCompat.OnRequestPermissionsResultCallback {

    private static final String TAG = MainActivity.class.getName();
    private static final int AUDIO_ECHO_REQUEST = 0;
    private static final int OBOE_API_AAUDIO = 0;
    private static final int OBOE_API_OPENSL_ES=1;

    private TextView statusText;
    private Button toggleEchoButton;
    private AudioDeviceSpinner recordingDeviceSpinner;
    private AudioDeviceSpinner playbackDeviceSpinner;
    private boolean isPlaying = false;

    private int apiSelection = OBOE_API_AAUDIO;
    private boolean aaudioSupported = true;
    private SeekBar mixerSeekbar, echoDelaySeekbar, echoDecaySeekbar;
    private TextView echoDelayTV, echoDecayTV;
    float   mixerProgress, echoDelayProgress, echoDecayProgress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        statusText = findViewById(R.id.status_view_text);
        toggleEchoButton = findViewById(R.id.button_toggle_echo);
        toggleEchoButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                toggleEcho();
            }
        });
        toggleEchoButton.setText(getString(R.string.start_echo));

        recordingDeviceSpinner = findViewById(R.id.recording_devices_spinner);
        recordingDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_INPUTS);
        recordingDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                EchoEngine.setRecordingDeviceId(getRecordingDeviceId());
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                // Do nothing
            }
        });

        playbackDeviceSpinner = findViewById(R.id.playback_devices_spinner);
        playbackDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_OUTPUTS);
        playbackDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                EchoEngine.setPlaybackDeviceId(getPlaybackDeviceId());
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                // Do nothing
            }
        });

        ((RadioGroup)findViewById(R.id.apiSelectionGroup)).check(R.id.aaudioButton);
        findViewById(R.id.aaudioButton).setOnClickListener(new RadioButton.OnClickListener(){
            @Override
            public void onClick(View v) {
                if (((RadioButton)v).isChecked()) {
                    apiSelection = OBOE_API_AAUDIO;
                }
            }
        });
        findViewById(R.id.slesButton).setOnClickListener(new RadioButton.OnClickListener(){
            @Override
            public void onClick(View v) {
                if (((RadioButton)v).isChecked()) {
                    apiSelection = OBOE_API_OPENSL_ES;
                }
            }
        });

        mixerSeekbar = findViewById(R.id.mixerSeekBar);
        mixerProgress = mixerSeekbar.getProgress()/((float)mixerSeekbar.getMax());
        mixerSeekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser) return;

                Log.d(TAG, "mixerSeekBar value = " + progress);
                mixerProgress = progress / (float)mixerSeekbar.getMax();
                EchoEngine.setMixer(mixerProgress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        echoDelaySeekbar = findViewById(R.id.echoDelaySeekBar);
        echoDelayTV = findViewById(R.id.echoCurDelay);
        echoDelayProgress = (float)echoDelaySeekbar.getProgress()
                             / echoDelaySeekbar.getMax();
        echoDelaySeekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {

                float curVal = (float)progress/seekBar.getMax();
                echoDelayTV.setText(String.format("%s", curVal));
                setSeekBarPromptPosition(echoDelaySeekbar, echoDelayTV);

                if (!fromUser) return;
                echoDelayProgress = curVal;
                EchoEngine.setEchoControls(echoDelayProgress, echoDecayProgress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
        echoDelaySeekbar.post(new Runnable() {
            @Override
            public void run() {
                setSeekBarPromptPosition(echoDelaySeekbar, echoDelayTV);
            }
        });

        echoDecaySeekbar = findViewById(R.id.echoDecaySeekBar);
        echoDecayTV = findViewById(R.id.echoCurDecay);
        echoDecayProgress = (float)echoDecaySeekbar.getProgress() / echoDecaySeekbar.getMax();
        echoDecaySeekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                float curVal = (float)progress / seekBar.getMax();
                echoDecayTV.setText(String.format("%s", curVal));
                setSeekBarPromptPosition(echoDecaySeekbar, echoDecayTV);
                if (!fromUser)
                    return;

                echoDecayProgress = curVal;
                EchoEngine.setEchoControls(echoDelayProgress, echoDecayProgress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        echoDecaySeekbar.post(new Runnable() {
            @Override
            public void run() {
                setSeekBarPromptPosition(echoDecaySeekbar, echoDecayTV);
            }
        });
        EchoEngine.create();
        aaudioSupported = EchoEngine.isAAudioSupported();
        EnableAudioApiUI(true);
        EchoEngine.setAPI(apiSelection);

        try {
            AssetManager assetManager = getAssets();
            String[] assets = assetManager.list("");
            int channels = 2;
            int frequency = 48000;
            EchoEngine.setStreamFile(assetManager, assets[1], channels, frequency);
            EchoEngine.setMixer(mixerProgress);
        } catch (IOException e) {
            Log.e(TAG, e.getMessage());
        }
        EchoEngine.setEchoControls(echoDelayProgress, echoDecayProgress);
    }
    private void setSeekBarPromptPosition(SeekBar seekBar, TextView label) {
        float thumbX = (float)seekBar.getProgress()/ seekBar.getMax() *
                seekBar.getWidth() + seekBar.getX();
        label.setX(thumbX - label.getWidth()/2.0f);
    }
    private void EnableAudioApiUI(boolean enable) {
        if(apiSelection == OBOE_API_AAUDIO && !aaudioSupported)
        {
            apiSelection = OBOE_API_OPENSL_ES;
        }
        findViewById(R.id.slesButton).setEnabled(enable);
        if(!aaudioSupported) {
          findViewById(R.id.aaudioButton).setEnabled(false);
        } else {
            findViewById(R.id.aaudioButton).setEnabled(enable);
        }

        ((RadioGroup)findViewById(R.id.apiSelectionGroup))
          .check(apiSelection == OBOE_API_AAUDIO ? R.id.aaudioButton : R.id.slesButton);
    }
    @Override
    protected void onStart() {
        super.onStart();
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
    }

    @Override
    protected void onDestroy() {
        EchoEngine.delete();
        super.onDestroy();
    }

    public void toggleEcho() {
        if (isPlaying) {
            stopEchoing();
            EnableAudioApiUI(true);
        } else {
            EnableAudioApiUI(false);
            EchoEngine.setAPI(apiSelection);
            startEchoing();
        }
    }

    private void startEchoing() {
        Log.d(TAG, "Attempting to start");

        if (!isRecordPermissionGranted()){
            requestRecordPermission();
            return;
        }

        setSpinnersEnabled(false);
        EchoEngine.setEchoControls(echoDelayProgress, echoDecayProgress);
        EchoEngine.setEchoOn(true);
        statusText.setText(R.string.status_echoing);
        toggleEchoButton.setText(R.string.stop_echo);
        isPlaying = true;
    }

    private void stopEchoing() {
        Log.d(TAG, "Playing, attempting to stop");
        EchoEngine.setEchoOn(false);
        resetStatusView();
        toggleEchoButton.setText(R.string.start_echo);
        isPlaying = false;
        setSpinnersEnabled(true);
    }

    private void setSpinnersEnabled(boolean isEnabled){
        recordingDeviceSpinner.setEnabled(isEnabled);
        playbackDeviceSpinner.setEnabled(isEnabled);
    }

    private int getRecordingDeviceId(){
        return ((AudioDeviceListEntry)recordingDeviceSpinner.getSelectedItem()).getId();
    }

    private int getPlaybackDeviceId(){
        return ((AudioDeviceListEntry)playbackDeviceSpinner.getSelectedItem()).getId();
    }

    private boolean isRecordPermissionGranted() {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
                PackageManager.PERMISSION_GRANTED);
    }

    private void requestRecordPermission(){
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.RECORD_AUDIO},
                AUDIO_ECHO_REQUEST);
    }
    private void resetStatusView() {
        statusText.setText(R.string.status_warning);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {

        if (AUDIO_ECHO_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1 ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {

            // User denied the permission, without this we cannot record audio
            // Show a toast and update the status accordingly
            statusText.setText(R.string.status_record_audio_denied);
            Toast.makeText(getApplicationContext(),
                    getString(R.string.need_record_audio_permission),
                    Toast.LENGTH_SHORT)
                    .show();
        } else {
            // Permission was granted, start echoing
            toggleEcho();
        }
    }
}
