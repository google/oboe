/*
 * Copyright 2026 The Android Open Source Project
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

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

/**
 * Activity to diagnose MMAP audio timestamp accuracy, DMA ring-buffer position reporting,
 * rapid start/pause/flush/stop cycles, and idle standby (>3s) resume behavior.
 */
public class TestMmapTimestampActivity extends AppCompatActivity {

    public static final String TAG = "OboeTester";
    public static final String KEY_AUTO_RUN = "auto_run";
    public static final String KEY_OUTPUT = "output";
    public static final String KEY_INPUT = "input";
    public static final String KEY_EXCLUSIVE = "exclusive";
    public static final String KEY_SHARED = "shared";
    public static final String KEY_STEADY = "steady";
    public static final String KEY_RAPID = "rapid";
    public static final String KEY_STANDBY = "standby";
    public static final String REPORT_FILENAME = "mmap_timestamp_report.txt";

    private static final int MY_PERMISSIONS_REQUEST_RECORD_AUDIO = 938356;

    static {
        System.loadLibrary("oboetester");
    }

    private TextView mStatusView;
    private ScrollView mScrollView;
    private CheckBox mOutputCheckBox;
    private CheckBox mInputCheckBox;
    private CheckBox mExclusiveCheckBox;
    private CheckBox mSharedCheckBox;
    private CheckBox mPhaseSteadyCheckBox;
    private CheckBox mPhaseRapidCyclesCheckBox;
    private CheckBox mPhaseStandbyCheckBox;
    private Button mStartButton;
    private Button mStopButton;
    private Button mShareButton;

    private volatile ReportPoller mPoller;
    private volatile String mDeviceHeader = "";
    private volatile String mReportFilePath;
    private volatile boolean mUiAlive;
    private boolean mAutoRun;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mUiAlive = true;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        }
        setContentView(R.layout.activity_mmap_timestamp);

        mStatusView = (TextView) findViewById(R.id.text_status);
        mScrollView = (ScrollView) findViewById(R.id.text_log_scroller);
        mOutputCheckBox = (CheckBox) findViewById(R.id.checkbox_mmap_dir_output);
        mInputCheckBox = (CheckBox) findViewById(R.id.checkbox_mmap_dir_input);
        mExclusiveCheckBox = (CheckBox) findViewById(R.id.checkbox_mmap_share_exclusive);
        mSharedCheckBox = (CheckBox) findViewById(R.id.checkbox_mmap_share_shared);
        mPhaseSteadyCheckBox = (CheckBox) findViewById(R.id.checkbox_phase_steady);
        mPhaseRapidCyclesCheckBox = (CheckBox) findViewById(R.id.checkbox_phase_rapid_cycles);
        mPhaseStandbyCheckBox = (CheckBox) findViewById(R.id.checkbox_phase_standby);

        mStartButton = (Button) findViewById(R.id.button_start_test);
        mStopButton = (Button) findViewById(R.id.button_stop_test);
        mShareButton = (Button) findViewById(R.id.button_share_report);

        mDeviceHeader = buildDeviceHeader();
        mStatusView.setText(styleReportForDisplay(mDeviceHeader
                + "\nTap START to run MMAP Timestamp & Position Diagnostic:\n"
                + "  [1] Steady-State & Cold-Start DMA Check\n"
                + "  [2] Rapid Start / Pause / Flush / Stop Cycles\n"
                + "  [3] MMAP Idle Standby (>3.5s) Resume Check\n"));

        setButtonsEnabled(false);

        applyIntentExtras(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        applyIntentExtras(intent);
    }

    private void applyIntentExtras(Intent intent) {
        if (intent == null) {
            return;
        }
        setIntent(intent);
        if (intent.hasExtra(KEY_OUTPUT)) {
            mOutputCheckBox.setChecked(intent.getBooleanExtra(KEY_OUTPUT, true));
        }
        if (intent.hasExtra(KEY_INPUT)) {
            mInputCheckBox.setChecked(intent.getBooleanExtra(KEY_INPUT, true));
        }
        if (intent.hasExtra(KEY_EXCLUSIVE)) {
            mExclusiveCheckBox.setChecked(intent.getBooleanExtra(KEY_EXCLUSIVE, true));
        }
        if (intent.hasExtra(KEY_SHARED)) {
            mSharedCheckBox.setChecked(intent.getBooleanExtra(KEY_SHARED, true));
        }
        if (intent.hasExtra(KEY_STEADY)) {
            mPhaseSteadyCheckBox.setChecked(intent.getBooleanExtra(KEY_STEADY, true));
        }
        if (intent.hasExtra(KEY_RAPID)) {
            mPhaseRapidCyclesCheckBox.setChecked(intent.getBooleanExtra(KEY_RAPID, true));
        }
        if (intent.hasExtra(KEY_STANDBY)) {
            mPhaseStandbyCheckBox.setChecked(intent.getBooleanExtra(KEY_STANDBY, true));
        }
        String customFile = intent.getStringExtra(IntentBasedTestSupport.KEY_FILE_NAME);
        mReportFilePath = (customFile != null && !customFile.isEmpty()) ? customFile : null;
        boolean launchedByTestIntent = intent.hasExtra(MainActivity.KEY_TEST_NAME);
        if (intent.hasExtra(KEY_AUTO_RUN)) {
            mAutoRun = intent.getBooleanExtra(KEY_AUTO_RUN, false);
        } else {
            mAutoRun = launchedByTestIntent;
        }
        intent.removeExtra(KEY_AUTO_RUN);
        intent.removeExtra(MainActivity.KEY_TEST_NAME);
        if (mAutoRun) {
            startTest();
        }
    }

    private String buildDeviceHeader() {
        return DiagnosticReportSupport.buildDeviceHeader();
    }

    public void onStartMmapTimestampTest(View view) {
        mAutoRun = false;
        startTest();
    }

    public void onStopMmapTimestampTest(View view) {
        stopTest();
    }

    public void onShareMmapTimestampReport(View view) {
        if (mStatusView != null) {
            DiagnosticReportSupport.sharePlainTextReport(this, mStatusView.getText());
        }
    }

    private void startTest() {
        ReportPoller existingPoller = mPoller;
        if (isRunningNative() || (existingPoller != null && existingPoller.isAlive())) {
            Log.w(TAG, "MMAP timestamp test is already running; ignoring duplicate start.");
            return;
        }

        boolean testOutput = mOutputCheckBox.isChecked();
        boolean testInput = mInputCheckBox.isChecked();
        boolean testExclusive = mExclusiveCheckBox.isChecked();
        boolean testShared = mSharedCheckBox.isChecked();
        boolean runSteadyState = mPhaseSteadyCheckBox.isChecked();
        boolean runRapidCycles = mPhaseRapidCyclesCheckBox.isChecked();
        boolean runStandbyResume = mPhaseStandbyCheckBox.isChecked();

        if ((!testOutput && !testInput)
                || (!testExclusive && !testShared)
                || (!runSteadyState && !runRapidCycles && !runStandbyResume)) {
            String msg = "Select at least one direction, sharing mode, and test phase";
            Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
            if (mAutoRun) {
                Log.e(TAG, "Invalid MMAP timestamp test configuration: " + msg);
                saveReportToFile(buildDeviceHeader() + "\nERROR: " + msg + "\n");
            }
            return;
        }

        mDeviceHeader = buildDeviceHeader();
        if (testInput && ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            if (!mAutoRun) {
                ActivityCompat.requestPermissions(
                        this,
                        new String[]{Manifest.permission.RECORD_AUDIO},
                        MY_PERMISSIONS_REQUEST_RECORD_AUDIO);
                return;
            } else {
                Log.w(TAG, "RECORD_AUDIO permission not granted during auto_run; skipping Input.");
                mDeviceHeader += "Note   : RECORD_AUDIO permission not granted (Input skipped)\n";
                testInput = false;
            }
        }

        keepScreenOn(true);

        int rc = startTestNative(testOutput, testInput, testExclusive, testShared,
                runSteadyState, runRapidCycles, runStandbyResume);
        if (rc < 0) {
            Log.w(TAG, "startTestNative returned " + rc + " (already running)");
            keepScreenOn(false);
            return;
        }

        setButtonsEnabled(true);
        ReportPoller poller = new ReportPoller();
        mPoller = poller;
        poller.start();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        if (requestCode != MY_PERMISSIONS_REQUEST_RECORD_AUDIO) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }
        if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startTest();
        } else {
            Toast.makeText(this, R.string.need_record_audio_permission, Toast.LENGTH_SHORT).show();
            mInputCheckBox.setChecked(false);
            if (mOutputCheckBox.isChecked()) {
                startTest();
            }
        }
    }

    private void stopTest() {
        ReportPoller poller = mPoller;
        if (!isRunningNative() && (poller == null || !poller.isAlive())) {
            mPoller = null;
            setButtonsEnabled(false);
            return;
        }
        keepScreenOn(false);
        stopTestNative();
        mStopButton.setEnabled(false);
        if (poller != null) {
            poller.wakeUp();
        }
    }

    private void setButtonsEnabled(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
        mShareButton.setEnabled(!running);
        mOutputCheckBox.setEnabled(!running);
        mInputCheckBox.setEnabled(!running);
        mExclusiveCheckBox.setEnabled(!running);
        mSharedCheckBox.setEnabled(!running);
        mPhaseSteadyCheckBox.setEnabled(!running);
        mPhaseRapidCyclesCheckBox.setEnabled(!running);
        mPhaseStandbyCheckBox.setEnabled(!running);
    }

    private class ReportPoller extends Thread {
        @Override
        public void run() {
            while (isRunningNative()) {
                if (mUiAlive) {
                    updateReportUI();
                }
                try {
                    sleep(200);
                } catch (InterruptedException e) {
                    // Woken early (e.g., stop requested); re-check isRunningNative().
                }
            }
            joinWorkerNative();
            final String finalReport = mDeviceHeader + "\n" + getReportNative();
            saveReportToFile(finalReport);
            if (mUiAlive) {
                updateReportUI();
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (mPoller == ReportPoller.this) {
                            mPoller = null;
                        }
                        if (!mUiAlive) {
                            return;
                        }
                        boolean running = isRunningNative();
                        keepScreenOn(running);
                        setButtonsEnabled(running);
                    }
                });
            } else if (mPoller == ReportPoller.this) {
                mPoller = null;
            }
        }

        void wakeUp() {
            interrupt();
        }
    }

    private void saveReportToFile(String report) {
        DiagnosticReportSupport.saveReportToExternalFile(
                this, mReportFilePath, REPORT_FILENAME, "MMAP timestamp report", report);
    }

    private SpannableString styleReportForDisplay(String text) {
        SpannableString span = new SpannableString(text);
        int len = text.length();
        int lineStart = 0;
        while (lineStart < len) {
            int lineEnd = text.indexOf('\n', lineStart);
            if (lineEnd < 0) {
                lineEnd = len;
            }
            String line = text.substring(lineStart, lineEnd);
            if (line.startsWith("====")
                    || line.startsWith("1. SUMMARY")
                    || line.startsWith("2. DETECTED")
                    || line.startsWith("3. GLOSSARY")
                    || line.startsWith("SCORECARD BY STREAM:")
                    || line.startsWith("STREAM: ")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                span.setSpan(new ForegroundColorSpan(DiagnosticReportSupport.COLOR_HEADER),
                        lineStart, lineEnd, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if (line.startsWith("[ISSUE #")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                span.setSpan(
                        new ForegroundColorSpan(DiagnosticReportSupport.issueHeaderColor(line)),
                        lineStart, lineEnd, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if (line.startsWith("* OUTPUT") || line.startsWith("* INPUT")
                    || line.startsWith("  [Phase ")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if (line.startsWith("- ") && line.indexOf(':') > 0
                    && line.indexOf(':') < 22) {
                int colonEnd = lineStart + line.indexOf(':') + 1;
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, colonEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
            lineStart = lineEnd + 1;
        }

        DiagnosticReportSupport.applyStatusSpans(span, text);
        return span;
    }

    private void updateReportUI() {
        final boolean running = isRunningNative();
        final String fullReport = mDeviceHeader + "\n" + getReportNative();
        final SpannableString styled = styleReportForDisplay(fullReport);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (!mUiAlive || mStatusView == null) {
                    return;
                }
                mStatusView.setText(styled);
                if (mScrollView != null && !running) {
                    mScrollView.scrollTo(0, 0);
                    mScrollView.post(() -> mScrollView.scrollTo(0, 0));
                }
            }
        });
    }

    @Override
    public void onPause() {
        super.onPause();
        stopTest();
    }

    @Override
    protected void onDestroy() {
        mUiAlive = false;
        stopTestNative();
        ReportPoller poller = mPoller;
        if (poller != null) {
            poller.wakeUp();
        }
        super.onDestroy();
    }

    protected void keepScreenOn(boolean on) {
        DiagnosticReportSupport.setKeepScreenOn(this, on);
    }

    private native int startTestNative(boolean testOutput, boolean testInput,
                                       boolean testExclusive, boolean testShared,
                                       boolean runSteadyState, boolean runRapidCycles,
                                       boolean runStandbyResume);
    private native int stopTestNative();
    private native int joinWorkerNative();
    private native boolean isRunningNative();
    private native int getResultNative();
    private native String getReportNative();
}
