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


import android.media.midi.MidiDeviceService;
import android.media.midi.MidiReceiver;
import android.util.Log;

import com.mobileer.miditools.MidiConstants;
import com.mobileer.miditools.MidiFramer;

import java.io.IOException;
import java.util.ArrayList;

/**
 * Measure the latency of various output paths by playing a blip.
 * Report the results back to the TestListeners.
 */
public class AudioMidiTester extends MidiDeviceService {

    // Sometimes the service can be run without the MainActivity being run!
    static {
        // Must match name in CMakeLists.txt
        System.loadLibrary("oboetester");
    }

    private static final float MAX_TOUCH_LATENCY = 0.200f;
    private static final float MAX_OUTPUT_LATENCY = 0.600f;
    private static final float ANALYSIS_TIME_MARGIN = 0.250f;

    private static final float ANALYSIS_TIME_DELAY = MAX_OUTPUT_LATENCY;
    private static final float ANALYSIS_TIME_TOTAL = MAX_TOUCH_LATENCY + MAX_OUTPUT_LATENCY;
    private static final float ANALYSIS_TIME_MAX = ANALYSIS_TIME_TOTAL + ANALYSIS_TIME_MARGIN;
    private static final int ANALYSIS_SAMPLE_RATE = 48000; // need not match output rate

    private ArrayList<TestListener> mListeners = new ArrayList<TestListener>();
    private MyMidiReceiver mReceiver = new MyMidiReceiver();
    private MidiFramer mMidiFramer = new MidiFramer(mReceiver);
    private boolean mRecordEnabled = true;

    private static AudioMidiTester mInstance;
    private AudioRecordThread mRecorder;
    private TapLatencyAnalyser mTapLatencyAnalyser;

    private AudioOutputTester mAudioOutputTester;

    public static class TestResult {
        public float[] samples;
        public float[] filtered;
        public int frameRate;
        public TapLatencyAnalyser.TapLatencyEvent[] events;
    }

    public static interface TestListener {
        public void onTestFinished(TestResult result);

        public void onNoteOn(int pitch);
    }

    /**
     * This is a Service so it is only created when a client requests the service.
     */
    public AudioMidiTester() {
        mInstance = this;
    }

    public void addTestListener(TestListener listener) {
        mListeners.add(listener);
    }

    public void removeTestListener(TestListener listener) {
        mListeners.remove(listener);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (mRecordEnabled) {
            mRecorder = new AudioRecordThread(ANALYSIS_SAMPLE_RATE,
                    1,
                    (int) (ANALYSIS_TIME_MAX * ANALYSIS_SAMPLE_RATE));
        }

        mAudioOutputTester = AudioOutputTester.getInstance();

        mTapLatencyAnalyser = new TapLatencyAnalyser();
    }

    @Override
    public void onDestroy() {
        // do stuff here
        super.onDestroy();
    }

    public static AudioMidiTester getInstance() {
        return mInstance;
    }

    class MyMidiReceiver extends MidiReceiver {
        public void onSend(byte[] data, int offset,
                           int count, long timestamp) throws IOException {
            // parse MIDI
            byte command = (byte) (data[0] & 0x0F0);
            if (command == MidiConstants.STATUS_NOTE_ON) {
                if (data[2] == 0) {
                    noteOff(data[1]);
                } else {
                    noteOn(data[1]);
                }
            } else if (command == MidiConstants.STATUS_NOTE_OFF) {
                noteOff(data[1]);
            }
            Log.i(TapToToneActivity.TAG, "MIDI command = " + command);
        }
    }

    private void noteOn(byte b) {
        setEnabled(true);
        fireNoteOn(b);
    }

    private void fireNoteOn(byte pitch) {
        for (TestListener listener : mListeners) {
            listener.onNoteOn(pitch);
        }
    }

    private void noteOff(byte b) {
        setEnabled(false);
    }

    @Override
    public MidiReceiver[] onGetInputPortReceivers() {
        return new MidiReceiver[]{mMidiFramer};
    }


    public void start() throws IOException {
        if (mRecordEnabled) {
            mRecorder.startAudio();
        }
    }

    public void setEnabled(boolean checked) {
        mAudioOutputTester.setEnabled(checked);
        if (checked && mRecordEnabled) {
            // schedule an analysis to start in the near future
            int numSamples = (int) (mRecorder.getSampleRate() * ANALYSIS_TIME_DELAY);
            Runnable task = new Runnable() {
                public void run() {
                    new Thread() {
                        public void run() {
                            analyzeCapturedAudio();
                        }
                    }.start();
                }
            };

            mRecorder.scheduleTask(numSamples, task);
        }
    }

    private void analyzeCapturedAudio() {
        if (!mRecordEnabled) return;
        int numSamples = (int) (mRecorder.getSampleRate() * ANALYSIS_TIME_TOTAL);
        float[] buffer = new float[numSamples];
        mRecorder.setCaptureEnabled(false); // TODO wait for it to settle
        int numRead = mRecorder.readMostRecent(buffer);

        TestResult result = new TestResult();
        result.samples = buffer;
        result.frameRate = mRecorder.getSampleRate();
        result.events = mTapLatencyAnalyser.analyze(buffer, 0, numRead);
        result.filtered = mTapLatencyAnalyser.getFilteredBuffer();
        mRecorder.setCaptureEnabled(true);
        // notify listeners
        for (TestListener listener : mListeners) {
            listener.onTestFinished(result);
        }
    }


    public void stop() {
        if (mRecordEnabled) {
            mRecorder.stopAudio();
        }
    }

}
