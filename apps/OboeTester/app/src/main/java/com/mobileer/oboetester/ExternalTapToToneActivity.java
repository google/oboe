package com.mobileer.oboetester;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import java.io.IOException;

public class ExternalTapToToneActivity extends Activity {
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

    public void startTest(View view) throws IOException {
        mTapToToneTester.resetLatency();
        mTapToToneTester.start();
        updateButtons(true);
    }

    public void stopTest(View view) {
        mTapToToneTester.stop();
        updateButtons(false);
    }

    @Override
    public void onStop() {
        mTapToToneTester.stop();
        super.onStop();
    }
}