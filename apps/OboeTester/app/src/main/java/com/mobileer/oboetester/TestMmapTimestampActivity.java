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

import static com.mobileer.oboetester.AudioQueryTools.getSystemProperty;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Activity to diagnose MMAP audio timestamp accuracy, DMA ring-buffer position reporting,
 * rapid start/pause/flush/stop cycles, and idle standby (>3s) resume behavior.
 */
public class TestMmapTimestampActivity extends AppCompatActivity {

    public static final String TAG = "OboeTester";
    public static final String KEY_AUTO_RUN = "auto_run";
    public static final String REPORT_FILENAME = "mmap_timestamp_report.txt";

    private static final int COLOR_HEADER = Color.parseColor("#0D47A1");
    private static final int COLOR_PASS = Color.parseColor("#2E7D32");
    private static final int COLOR_WARN = Color.parseColor("#E65100");
    private static final int COLOR_FAIL = Color.parseColor("#C62828");

    private static final Pattern PATTERN_PASS =
            Pattern.compile("\\bPASS\\b|\\[OK\\s*\\]");
    private static final Pattern PATTERN_WARN =
            Pattern.compile("(?<!\\b0 )\\bWARN(?:ING)?(?: \\([^)\\n]+\\))?");
    private static final Pattern PATTERN_FAIL =
            Pattern.compile("(?<!\\b0 )\\bFAIL(?: \\([^)\\n]+\\))?|\\[FAIL\\]");

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

    private ReportPoller mPoller;
    private String mDeviceHeader;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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

        Intent intent = getIntent();
        if (intent != null && intent.getBooleanExtra(KEY_AUTO_RUN, false)) {
            startTest();
        }
    }

    private String buildDeviceHeader() {
        StringBuilder sb = new StringBuilder();
        sb.append("Device : ").append(Build.MANUFACTURER)
                .append(" ").append(Build.MODEL)
                .append(" (").append(Build.PRODUCT).append(")\n");
        sb.append("SoC    : ").append(getSystemProperty("ro.soc.model"))
                .append(" (board=").append(Build.HARDWARE)
                .append(", ").append(getSystemProperty("ro.board.platform")).append(")\n");
        sb.append("Build  : ").append(Build.ID)
                .append(" | ").append(Build.TYPE)
                .append(" (SDK ").append(Build.VERSION.SDK_INT).append(")\n");
        return sb.toString();
    }

    public void onStartMmapTimestampTest(View view) {
        startTest();
    }

    public void onStopMmapTimestampTest(View view) {
        stopTest();
    }

    public void onShareMmapTimestampReport(View view) {
        if (mStatusView != null) {
            Intent sendIntent = new Intent();
            sendIntent.setAction(Intent.ACTION_SEND);
            sendIntent.putExtra(Intent.EXTRA_TEXT, mStatusView.getText().toString());
            sendIntent.setType("text/plain");
            Intent shareIntent = Intent.createChooser(sendIntent, null);
            startActivity(shareIntent);
        }
    }

    private void startTest() {
        keepScreenOn(true);
        stopPoller();

        boolean testOutput = mOutputCheckBox.isChecked();
        boolean testInput = mInputCheckBox.isChecked();
        boolean testExclusive = mExclusiveCheckBox.isChecked();
        boolean testShared = mSharedCheckBox.isChecked();
        boolean runSteadyState = mPhaseSteadyCheckBox.isChecked();
        boolean runRapidCycles = mPhaseRapidCyclesCheckBox.isChecked();
        boolean runStandbyResume = mPhaseStandbyCheckBox.isChecked();

        startTestNative(testOutput, testInput, testExclusive, testShared,
                runSteadyState, runRapidCycles, runStandbyResume);

        setButtonsEnabled(true);
        mPoller = new ReportPoller();
        mPoller.start();
    }

    private void stopTest() {
        keepScreenOn(false);
        stopTestNative();
        stopPoller();
        updateReportUI();
        setButtonsEnabled(false);
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
        volatile boolean mEnabled = true;

        @Override
        public void run() {
            while (mEnabled && isRunningNative()) {
                updateReportUI();
                try {
                    sleep(200);
                } catch (InterruptedException e) {
                    break;
                }
            }
            if (!mEnabled) {
                return;
            }
            final String finalReport = mDeviceHeader + "\n" + getReportNative();
            saveReportToFile(finalReport);
            updateReportUI();
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    boolean running = isRunningNative();
                    keepScreenOn(running);
                    setButtonsEnabled(running);
                }
            });
        }

        void finish() {
            mEnabled = false;
            interrupt();
            try {
                join(1000);
            } catch (InterruptedException e) {
                // Ignore
            }
        }
    }

    private void saveReportToFile(String report) {
        File dir = getExternalFilesDir(null);
        if (dir == null) {
            return;
        }
        File outFile = new File(dir, REPORT_FILENAME);
        try (FileOutputStream fos = new FileOutputStream(outFile)) {
            fos.write(report.getBytes(StandardCharsets.UTF_8));
            Log.i(TAG, "Saved MMAP timestamp report to: " + outFile.getAbsolutePath());
        } catch (IOException e) {
            Log.e(TAG, "Failed to write MMAP timestamp report", e);
        }
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
                span.setSpan(new ForegroundColorSpan(COLOR_HEADER), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if (line.startsWith("[ISSUE #")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                span.setSpan(new ForegroundColorSpan(COLOR_FAIL), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
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

        applyPatternSpan(span, text, PATTERN_PASS, COLOR_PASS);
        applyPatternSpan(span, text, PATTERN_WARN, COLOR_WARN);
        applyPatternSpan(span, text, PATTERN_FAIL, COLOR_FAIL);
        return span;
    }

    private void applyPatternSpan(SpannableString span, String text, Pattern pattern, int color) {
        Matcher m = pattern.matcher(text);
        while (m.find()) {
            span.setSpan(new StyleSpan(Typeface.BOLD), m.start(), m.end(),
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            span.setSpan(new ForegroundColorSpan(color), m.start(), m.end(),
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    private void updateReportUI() {
        final boolean running = isRunningNative();
        final String fullReport = mDeviceHeader + "\n" + getReportNative();
        final SpannableString styled = styleReportForDisplay(fullReport);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStatusView.setText(styled);
                if (mScrollView != null && !running) {
                    mScrollView.fullScroll(View.FOCUS_UP);
                }
            }
        });
    }

    private void stopPoller() {
        if (mPoller != null) {
            mPoller.finish();
            mPoller = null;
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        stopTest();
    }

    protected void keepScreenOn(boolean on) {
        if (on) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }

    private native int startTestNative(boolean testOutput, boolean testInput,
                                       boolean testExclusive, boolean testShared,
                                       boolean runSteadyState, boolean runRapidCycles,
                                       boolean runStandbyResume);
    private native int stopTestNative();
    private native boolean isRunningNative();
    private native int getResultNative();
    private native String getReportNative();
}
