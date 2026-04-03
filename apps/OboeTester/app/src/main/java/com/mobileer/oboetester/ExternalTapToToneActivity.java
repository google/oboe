package com.mobileer.oboetester;

import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.io.IOException;

/**
 * Measure the tap-to-tone latency for other apps or devices.
 */
public class ExternalTapToToneActivity extends AppCompatActivity {
    private static final String TAG = "OboeTester";

    protected TapToToneTester mTapToToneTester;
    private Button mStopButton;
    private Button mStartButton;
    private Button mAnalyzeButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_external_tap_to_tone);

        mTapToToneTester = new TapToToneTester(this,
                getResources().getString(R.string.external_tap_instructions));

        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mAnalyzeButton = (Button) findViewById(R.id.button_analyze);

        WaveformView mWaveformView = (WaveformView) findViewById(R.id.waveview_audio_original);
        WaveformView mFastWaveformView = (WaveformView) findViewById(R.id.waveview_audio_fast_avg);
        WaveformView mSlowWaveformView = (WaveformView) findViewById(R.id.waveview_audio_slow_avg);
        WaveformView mLowThresholdWaveformView = (WaveformView) findViewById(R.id.waveview_audio_lowThreshold);
        WaveformView mArmedWaveformView = (WaveformView) findViewById(R.id.waveview_audio_armed_waveform);

        update(R.id.waveview_audio_original, Color.BLUE, Color.argb(128,0, 120, 0), Color.TRANSPARENT);
        update(R.id.waveview_audio_fast_avg, Color.argb(255,0, 247, 255), Color.TRANSPARENT, Color.argb(70, 255, 238, 0));
        update(R.id.waveview_audio_slow_avg, Color.argb(255, 174, 0, 255), Color.TRANSPARENT, Color.argb(70, 255, 238, 0));
        update(R.id.waveview_audio_lowThreshold, Color.argb(255, 255, 132, 0), Color.TRANSPARENT, Color.argb(70, 255, 238, 0));
        update(R.id.waveview_audio_armed_waveform, Color.argb(50, 255, 238, 0), Color.TRANSPARENT, Color.RED);
        updateButtons(false);
    }

    private void updateButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mAnalyzeButton.setEnabled(running);
        mStopButton.setEnabled(running);
    }

    public void analyseAndShowResults() {
        TapToToneTester.TestResult result = mTapToToneTester.analyzeCapturedAudio();
        if (result != null) {
            mTapToToneTester.showTestResults(result);
        }
    }

    public void analyze(View view) {
        analyseAndShowResults();
    }

    public void startTest(View view)  {
        try {
            mTapToToneTester.resetLatency();
            mTapToToneTester.start();
            updateButtons(true);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Start audio failed! " + e.getMessage());
            return;
        }
    }

    public void stopTest(View view) {
        mTapToToneTester.stop();
        updateButtons(false);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    public void onStop() {
        mTapToToneTester.stop();
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onStop();
    }


    protected void showErrorToast(String message) {
        showToast("Error: " + message);
    }

    protected void showToast(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(ExternalTapToToneActivity.this,
                        message,
                        Toast.LENGTH_SHORT).show();
            }
        });
    }

    private void update(int waveformViewId, int waveColor, int backgroundColor, int cursorColor) {
        WaveformView waveformView = (WaveformView) findViewById(waveformViewId);
        waveformView.updateTheme(waveColor, backgroundColor, cursorColor);
    }
}
