package com.mobileer.oboetester;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import java.io.IOException;

/**
 * Measure the tap-to-tone latency for other apps or devices.
 */
public class ExternalTapToToneActivity extends Activity {
    private static final String TAG = "OboeTester";
    private static final int MY_PERMISSIONS_REQUEST_RECORD_AUDIO = 1235;

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

    public void startTest(View view)  throws IOException {
        if (hasRecordAudioPermission()) {
            startAudioPermitted();
        } else {
            requestRecordAudioPermission();
            updateButtons(false);
        }
    }

    private void startAudioPermitted() {
        try {
            mTapToToneTester.resetLatency();
            mTapToToneTester.start();
            updateButtons(true);
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Start audio failed! " + e.getMessage());
            return;
        }
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
                                           String[] permissions,
                                           int[] grantResults) {

        if (MY_PERMISSIONS_REQUEST_RECORD_AUDIO != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1 ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            Toast.makeText(getApplicationContext(),
                    getString(R.string.need_record_audio_permission),
                    Toast.LENGTH_SHORT)
                    .show();
        } else {
            startAudioPermitted();
        }
    }

}
