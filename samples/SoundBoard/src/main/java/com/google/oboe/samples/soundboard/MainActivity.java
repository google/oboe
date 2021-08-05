package com.google.oboe.samples.soundboard;

/*
 * Copyright 2021 The Android Open Source Project
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

import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;

import java.util.ArrayList;

import static java.lang.Math.min;

public class MainActivity extends AppCompatActivity {

    private final String TAG = MainActivity.class.toString();
    private final int NUM_ROWS = 6;
    private final int NUM_COLUMNS = 5;
    private static long mEngineHandle = 0;

    private native long startEngine(int numSignals);
    private native void stopEngine(long engineHandle);

    private static native void native_setDefaultStreamValues(int sampleRate, int framesPerBurst);

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("soundboard");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }

    @Override
    protected void onResume(){
        setDefaultStreamValues(this);
        mEngineHandle = startEngine(NUM_ROWS * NUM_COLUMNS);
        createMusicTiles(this);
        super.onResume();
    }

    @Override
    protected void onPause(){
        stopEngine(mEngineHandle);
        super.onPause();
    }

    static void setDefaultStreamValues(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1){
            AudioManager myAudioMgr = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            String sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            int defaultSampleRate = Integer.parseInt(sampleRateStr);
            String framesPerBurstStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            int defaultFramesPerBurst = Integer.parseInt(framesPerBurstStr);

            native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst);
        }
    }

    void createMusicTiles(Context context) {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((Activity)context).getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        int height = displayMetrics.heightPixels;
        int width = displayMetrics.widthPixels;

        // 5 by 6 tiles
        final int numRows = NUM_ROWS;
        final int numColumns = NUM_COLUMNS;
        final int tileLength = min(height / (numRows), width / (numColumns));
        final int xStartLocation = (width - tileLength * numColumns) / 2;
        // Height isn't a perfect measurement so shift the location slightly up from the "center"
        final int yStartLocation = (height - tileLength * numRows) / 2 / 2;

        ArrayList<Rect> rectangles = new ArrayList<Rect>();
        for (int i = 0; i < numRows; i++) {
            for (int j = 0; j < numColumns; j++) {
                Rect rectangle = new Rect(xStartLocation + j * tileLength,
                        yStartLocation + i * tileLength,
                        xStartLocation + j * tileLength + tileLength,
                        yStartLocation + i * tileLength + tileLength);
                rectangles.add(rectangle);
            }
        }
        
        setContentView(new MusicTileView(this, rectangles, new NoteListener(mEngineHandle)));
    }
}


