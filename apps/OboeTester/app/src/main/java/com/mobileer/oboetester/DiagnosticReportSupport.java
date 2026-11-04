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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Build;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.util.Log;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Shared report formatting, external-files path confinement, sharing, and screen-wake utilities
 * for OboeTester diagnostic activities ({@link TestMmapTimestampActivity} and
 * {@link TestVoiceCommAecActivity}).
 */
final class DiagnosticReportSupport {

    static final String TAG = "OboeTester";

    static final int COLOR_HEADER = Color.parseColor("#0D47A1");
    static final int COLOR_PASS = Color.parseColor("#2E7D32");
    static final int COLOR_WARN = Color.parseColor("#E65100");
    static final int COLOR_FAIL = Color.parseColor("#C62828");

    private static final Pattern PATTERN_PASS =
            Pattern.compile("(?<!\\b0 )\\bPASS\\b|\\[OK\\s*\\]");
    private static final Pattern PATTERN_WARN =
            Pattern.compile("(?<!\\b0 )\\bWARN(?:ING)?(?: \\([^)\\n]+\\))?"
                    + "|\\bDISCONNECTED(?: \\([^)\\n]+\\))?"
                    + "|\\bSTOPPED(?: \\([^)\\n]+\\))?");
    private static final Pattern PATTERN_FAIL =
            Pattern.compile("(?<!\\b0 )\\bFAIL(?: \\([^)\\n]+\\))?|\\[FAIL\\]");

    private DiagnosticReportSupport() {}

    static String buildDeviceHeader() {
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

    static int issueHeaderColor(String line) {
        if (line.contains("(INFO)")) {
            return COLOR_HEADER;
        }
        return line.contains("(WARNING)") ? COLOR_WARN : COLOR_FAIL;
    }

    static void applyStatusSpans(SpannableString span, String text) {
        applyPatternSpan(span, text, PATTERN_PASS, COLOR_PASS);
        applyPatternSpan(span, text, PATTERN_WARN, COLOR_WARN);
        applyPatternSpan(span, text, PATTERN_FAIL, COLOR_FAIL);
    }

    private static void applyPatternSpan(
            SpannableString span, String text, Pattern pattern, int color) {
        Matcher m = pattern.matcher(text);
        while (m.find()) {
            span.setSpan(new StyleSpan(Typeface.BOLD), m.start(), m.end(),
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            span.setSpan(new ForegroundColorSpan(color), m.start(), m.end(),
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    static void saveReportToExternalFile(
            Context context,
            String requestedPath,
            String defaultFileName,
            String reportLabel,
            String report) {
        File dir = context.getExternalFilesDir(null);
        if (dir == null) {
            Log.e(TAG, "External files directory unavailable; cannot save " + reportLabel + ".");
            return;
        }
        File outFile;
        try {
            String canonicalDir = dir.getCanonicalPath();
            String canonicalPrefix = canonicalDir.endsWith(File.separator)
                    ? canonicalDir
                    : canonicalDir + File.separator;
            if (requestedPath != null && !requestedPath.trim().isEmpty()) {
                if (requestedPath.contains("..")) {
                    Log.e(TAG, "Rejecting unsafe report file path with '..': " + requestedPath);
                    return;
                }
                File raw = new File(requestedPath);
                File candidate = raw.isAbsolute() ? raw : new File(dir, requestedPath);
                String canonicalCandidate = candidate.getCanonicalPath();
                if (!canonicalCandidate.startsWith(canonicalPrefix)) {
                    Log.e(TAG, "Rejecting report file path outside external files dir: "
                            + requestedPath);
                    return;
                }
                outFile = candidate;
            } else {
                outFile = new File(dir, defaultFileName);
            }
            File parent = outFile.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                Log.e(TAG, "Failed to create parent directory: " + parent.getAbsolutePath());
                return;
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to resolve report file path", e);
            return;
        }
        if (outFile.exists()) {
            outFile.delete();
        }
        try (FileOutputStream fos = new FileOutputStream(outFile)) {
            fos.write(report.getBytes(StandardCharsets.UTF_8));
            Log.d(TAG, "EXTFILE = " + outFile.getAbsolutePath());
            Log.i(TAG, "Saved " + reportLabel + " to: " + outFile.getAbsolutePath());
        } catch (IOException e) {
            Log.e(TAG, "Failed to write " + reportLabel, e);
        }
    }

    static void sharePlainTextReport(Activity activity, CharSequence reportText) {
        if (reportText == null) {
            return;
        }
        Intent sendIntent = new Intent();
        sendIntent.setAction(Intent.ACTION_SEND);
        sendIntent.putExtra(Intent.EXTRA_TEXT, reportText.toString());
        sendIntent.setType("text/plain");
        Intent shareIntent = Intent.createChooser(sendIntent, null);
        activity.startActivity(shareIntent);
    }

    static void setKeepScreenOn(Activity activity, boolean on) {
        if (on) {
            activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }
}
