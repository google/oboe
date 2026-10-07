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

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.media.MediaCodec;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
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
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Activity to diagnose and tune the full-duplex Voice Communication (VoIP & Voice Agent) path:
 * <ul>
 *   <li>4-Phase Acoustic Echo Cancellation (AEC) verification (Ambient Noise Floor, Raw Coupling,
 *       VoIP AEC OFF Bypass, VoIP AEC ON ERLE & Envelope Correlation).</li>
 *   <li>Round-Trip Latency (RTL) attribution comparing {@code VOICE_RECOGNITION} vs.
 *       {@code VOICE_COMMUNICATION} and Native AAudio (MMAP/FastTrack) vs. Java AudioTrack/Record.
 *       Works even when hardware AEC is active by combining waveform cross-correlation in
 *       Phases 2 & 3 with {@code CLOCK_MONOTONIC} hardware timestamp latency across Phases 2, 3,
 *       and 4.</li>
 *   <li>Multi-turn voice agent standby (>3.5s idle pause + resume) verification on the
 *       {@code VOICE_COMMUNICATION} + {@code AcousticEchoCanceler} path.</li>
 * </ul>
 */
public class TestVoiceCommAecActivity extends AppCompatActivity {

    public static final String TAG = "OboeTester";
    public static final String KEY_AUTO_RUN = "auto_run";
    public static final String REPORT_FILENAME = "voice_comm_aec_report.txt";

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

    private static final int SAMPLE_RATE = 48000;
    private static final float MAX_VAL = 32768.0f;
    private static final float MIN_RMS_DB = -60.0f;
    private static final float MIN_RMS_VAL = (float) Math.pow(10.0, MIN_RMS_DB / 20.0);

    private static final int AAUDIO_INPUT_PRESET_VOICE_RECOGNITION = 6;
    private static final int AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION = 7;

    private static final int PHASE1_SILENCE_MS = 1500;
    private static final int PHASE_SPEECH_MS = 5000;
    private static final int SHOT_INTERVAL_MS = 200;
    private static final int STANDBY_PAUSE_MS = 3500;
    private static final int STANDBY_RESUME_MS = 1200;
    private static final int PLAYER_RMS_WINDOW_SAMPLES = 960; // 20 ms at 48 kHz

    // Diagnostic thresholds (aligned with 4-Phase CTS Verifier AudioAEC & 100ms Voice Agent target)
    private static final double THRESHOLD_AMBIENT_NOISE_CEILING_DBFS = -35.0;
    private static final double THRESHOLD_STIMULUS_SNR_DB = 12.0;
    private static final double THRESHOLD_CORR_AEC_OFF_MIN = 0.60;
    private static final double THRESHOLD_CORR_AEC_ON_MAX = 0.50;
    private static final double THRESHOLD_BYPASS_DELTA_DB = 6.0;
    private static final double THRESHOLD_ERLE_DB = 15.0;
    private static final double THRESHOLD_DIST_TO_FLOOR_DB = 6.0;
    private static final double THRESHOLD_MIN_ERLE_AT_FLOOR_DB = 8.0;
    private static final double THRESHOLD_VOICE_AGENT_RTL_MS = 100.0;

    static {
        System.loadLibrary("oboetester");
    }

    private TextView mStatusView;
    private ScrollView mScrollView;
    private CheckBox mNativeApiCheckBox;
    private CheckBox mJavaApiCheckBox;
    private CheckBox mForceNsOffCheckBox;
    private CheckBox mPhaseAecCheckBox;
    private CheckBox mPhaseStandbyCheckBox;
    private Button mStartButton;
    private Button mStopButton;
    private Button mPlayOffButton;
    private Button mPlayOnButton;
    private Button mShareButton;

    private final Object mReportLock = new Object();
    private final Object mSpeechDecodeLock = new Object();
    private short[] mSpeechPcm;
    private int mSpeechSampleRate = SAMPLE_RATE;

    private volatile boolean mWorkerEnabled = false;
    private volatile boolean mIsRunning = false;
    private Thread mWorkerThread;
    private ReportPoller mPoller;

    private String mDeviceHeader = "";
    private String mCurrentProgress = "Idle";
    private final StringBuilder mTelemetryLog = new StringBuilder();
    private final List<ApiRunMetrics> mScorecard = new ArrayList<>();
    private final List<Finding> mFindings = new ArrayList<>();
    private boolean mSuiteCompleted = false;
    private boolean mSuiteStoppedByUser = false;
    private int mPassCount = 0;
    private int mWarnCount = 0;
    private int mFailCount = 0;

    private boolean mAecSupported = false;
    private boolean mNsSupported = false;
    private boolean mMmapSupported = false;
    private boolean mMmapExclusiveSupported = false;

    private volatile short[] mLastRecordingAecOff;
    private volatile int mLastRecordingAecOffLen = 0;
    private volatile short[] mLastRecordingAecOn;
    private volatile int mLastRecordingAecOnLen = 0;
    private volatile String mLastRecordingApiLabel = "";

    private static class ApiRunMetrics {
        final boolean isNative;
        final String apiLabel;
        String configSummary = "Pending...";

        String phase1Status = "PENDING";
        String phase2Status = "PENDING";
        String phase3Status = "PENDING";
        String phase4Status = "PENDING";
        String phase5Status = "PENDING";

        boolean rawOutMmap = false;
        boolean rawInMmap = false;
        boolean voipOutMmap = false;
        boolean voipInMmap = false;
        boolean voipOutExclusive = false;
        boolean voipInExclusive = false;
        int outBurstFrames = 0;
        int outCapacityFrames = 0;
        int voipInBurstFrames = 0;
        int voipInCapacityFrames = 0;

        double noiseFloorDbfs = -99.0;
        double rawCouplingDbfs = -99.0;
        double stimulusSnrDb = 0.0;
        double aecOffDbfs = -99.0;
        double aecOnDbfs = -99.0;
        double scaledAecOffDbfs = -99.0;
        double scaledAecOnDbfs = -99.0;
        double voipGainScaleDb = 0.0;  // rawCouplingDbfs - aecOffDbfs (applied to VoIP levels)

        double rawErleDb = 0.0;        // aecOffDbfs - aecOnDbfs (unscaled)
        double selfErleDb = 0.0;       // scaledAecOffDbfs - scaledAecOnDbfs (gain-normalized)
        double peakRefErleDb = 0.0;    // max(rawCouplingDbfs, scaledAecOffDbfs) - scaledAecOnDbfs
        double bypassDeltaDb = 0.0;    // |aecOffDbfs - rawCouplingDbfs|
        double signedGainDeltaDb = 0.0; // aecOffDbfs - rawCouplingDbfs
        double distToFloorDb = 0.0;    // scaledAecOnDbfs - noiseFloorDbfs (gain-normalized)

        double corrAecOff = 0.0;
        double corrAecOn = 0.0;

        double rawAcousticRtlMs = -1.0;
        double rawTimestampRtlMs = -1.0;
        double voipOffAcousticRtlMs = -1.0;
        double voipOffTimestampRtlMs = -1.0;
        double voipOnTimestampRtlMs = -1.0;

        boolean standbyExecuted = false;
        boolean standbyPassed = false;
        long standbyFirstFrameMs = -1;
        long standbyPreTsPos = -1;
        long standbyPostTsPos = -1;
        double standbyPostRmsDbfs = -99.0;
        boolean standbyAecStillEnabled = false;

        boolean metricsPopulated = false;

        ApiRunMetrics(boolean isNative, String apiLabel) {
            this.isNative = isNative;
            this.apiLabel = apiLabel;
        }
    }

    private static class Finding {
        String id;
        String severity; // "FAIL" or "WARNING"
        String title;
        final List<String> affectedPaths = new ArrayList<>();
        String whatHappened;
        final List<String> measuredEvidence = new ArrayList<>();
        String androidContract;
        String whyItMatters;
        String howToFix;
    }

    private static class RmsEnvelopeTracker {
        private final int mWindowSamples;
        private final double[] mSnapshots;
        private final double[] mPreemphSnapshots;
        private int mShotIndex = 0;
        private double mRmsCurrent = MIN_RMS_VAL;
        private double mRmsPreemphCurrent = MIN_RMS_VAL;
        private double mEnergyAccumulator = 0.0;
        private double mPreemphEnergyAccumulator = 0.0;
        private double mPrevSample = 0.0;
        private int mEnergyCount = 0;
        private double mGainScale = 1.0;
        private volatile boolean mRunning = false;

        RmsEnvelopeTracker(int sampleRate, int maxShots) {
            mWindowSamples = Math.max(256, (sampleRate * SHOT_INTERVAL_MS) / 1000);
            mSnapshots = new double[maxShots];
            mPreemphSnapshots = new double[maxShots];
        }

        synchronized void setGainScale(double gainScale) {
            mGainScale = (gainScale > 0.0) ? gainScale : 1.0;
        }

        synchronized void reset() {
            Arrays.fill(mSnapshots, MIN_RMS_VAL);
            Arrays.fill(mPreemphSnapshots, MIN_RMS_VAL);
            mShotIndex = 0;
            mRmsCurrent = MIN_RMS_VAL;
            mRmsPreemphCurrent = MIN_RMS_VAL;
            mEnergyAccumulator = 0.0;
            mPreemphEnergyAccumulator = 0.0;
            mPrevSample = 0.0;
            mEnergyCount = 0;
            mRunning = false;
        }

        synchronized void setRunning(boolean running) {
            mRunning = running;
        }

        synchronized void updateDirect(short[] samples, int numSamples) {
            if (!mRunning || samples == null || numSamples <= 0) return;
            for (int i = 0; i < numSamples; i++) {
                double raw = samples[i] / MAX_VAL;
                double v = raw * mGainScale;
                double hp = (raw - 0.82 * mPrevSample) * mGainScale;
                mPrevSample = raw;
                mEnergyAccumulator += v * v;
                mPreemphEnergyAccumulator += hp * hp;
                mEnergyCount++;
            }
        }

        synchronized void captureShot() {
            if (mEnergyCount > 0) {
                double rms = Math.max(Math.sqrt(mEnergyAccumulator / mEnergyCount), MIN_RMS_VAL);
                double hpRms =
                        Math.max(Math.sqrt(mPreemphEnergyAccumulator / mEnergyCount), MIN_RMS_VAL);
                double alpha = 0.8;
                mRmsCurrent =
                        (mShotIndex == 0) ? rms : (rms * alpha + mRmsCurrent * (1.0 - alpha));
                mRmsPreemphCurrent =
                        (mShotIndex == 0)
                                ? hpRms
                                : (hpRms * alpha + mRmsPreemphCurrent * (1.0 - alpha));
                mEnergyAccumulator = 0.0;
                mPreemphEnergyAccumulator = 0.0;
                mEnergyCount = 0;
            }
            if (mShotIndex >= 0 && mShotIndex < mSnapshots.length) {
                mSnapshots[mShotIndex] = mRmsCurrent;
                mPreemphSnapshots[mShotIndex] = mRmsPreemphCurrent;
                mShotIndex++;
            }
        }

        synchronized int getShotCount() {
            return mShotIndex;
        }

        synchronized double getCurrentDbfs() {
            return 20.0 * Math.log10(Math.max(mRmsCurrent, MIN_RMS_VAL));
        }

        synchronized double[] getSnapshotsCopy() {
            return Arrays.copyOf(mSnapshots, mShotIndex);
        }

        synchronized double[] getPreemphSnapshotsCopy() {
            return Arrays.copyOf(mPreemphSnapshots, mShotIndex);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        }
        setContentView(R.layout.activity_voice_comm_aec);

        mStatusView = findViewById(R.id.text_status);
        mScrollView = findViewById(R.id.text_log_scroller);
        mNativeApiCheckBox = findViewById(R.id.checkbox_voip_api_native);
        mJavaApiCheckBox = findViewById(R.id.checkbox_voip_api_java);
        mForceNsOffCheckBox = findViewById(R.id.checkbox_voip_force_ns_off);
        mPhaseAecCheckBox = findViewById(R.id.checkbox_voip_phase_aec);
        mPhaseStandbyCheckBox = findViewById(R.id.checkbox_voip_phase_standby);

        mStartButton = findViewById(R.id.button_start_test);
        mStopButton = findViewById(R.id.button_stop_test);
        mPlayOffButton = findViewById(R.id.button_play_aec_off);
        mPlayOnButton = findViewById(R.id.button_play_aec_on);
        mShareButton = findViewById(R.id.button_share_report);

        mAecSupported = AcousticEchoCanceler.isAvailable();
        mNsSupported = NoiseSuppressor.isAvailable();
        mMmapSupported = nativeIsMMapSupported();
        mMmapExclusiveSupported = nativeIsMMapExclusiveSupported();

        mDeviceHeader = buildDeviceHeader();
        mStatusView.setText(styleReportForDisplay(mDeviceHeader
                + "\nTap START to run VoIP & AEC Diagnostic (no external rig needed):\n"
                + "  [1] Ambient Noise Floor (VOICE_RECOGNITION Silence)\n"
                + "  [2] Raw Coupling & Baseline RTL (VOICE_RECOGNITION Speech)\n"
                + "  [3] VoIP Bypass & VoIP RTL (VOICE_COMMUNICATION, AEC OFF)\n"
                + "  [4] Active AEC ERLE & Envelope Correlation (AEC ON)\n"
                + "  [5] Multi-Turn Voice Agent Standby (>3.5s Pause + Resume)\n"));

        setButtonsEnabled(false);

        new Thread(this::ensureSpeechPcmDecoded).start();

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

    public void onStartVoiceCommAecTest(View view) {
        startTest();
    }

    public void onStopVoiceCommAecTest(View view) {
        stopTest();
    }

    public void onPlayAecOffRecording(View view) {
        playCapturedBuffer(mLastRecordingAecOff, mLastRecordingAecOffLen);
    }

    public void onPlayAecOnRecording(View view) {
        playCapturedBuffer(mLastRecordingAecOn, mLastRecordingAecOnLen);
    }

    public void onShareVoiceCommAecReport(View view) {
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
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                    this, new String[]{Manifest.permission.RECORD_AUDIO}, 1001);
            return;
        }
        stopTest();
        keepScreenOn(true);

        final boolean testNative = mNativeApiCheckBox.isChecked();
        final boolean testJava = mJavaApiCheckBox.isChecked();
        final boolean forceNsOff = mForceNsOffCheckBox.isChecked();
        final boolean run4PhaseAec = mPhaseAecCheckBox.isChecked();
        final boolean runStandby = mPhaseStandbyCheckBox.isChecked();

        synchronized (mReportLock) {
            mTelemetryLog.setLength(0);
            mScorecard.clear();
            mFindings.clear();
            mSuiteCompleted = false;
            mSuiteStoppedByUser = false;
            mPassCount = 0;
            mWarnCount = 0;
            mFailCount = 0;
            mCurrentProgress = "Preparing VoIP & AEC Diagnostic...";

            if (testNative) {
                ApiRunMetrics row = new ApiRunMetrics(true, "NATIVE AAUDIO");
                row.phase1Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase2Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase3Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase4Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase5Status = runStandby ? "PENDING" : "SKIP";
                mScorecard.add(row);
            }
            if (testJava) {
                ApiRunMetrics row = new ApiRunMetrics(false, "JAVA API");
                row.phase1Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase2Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase3Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase4Status = run4PhaseAec ? "PENDING" : "SKIP";
                row.phase5Status = runStandby ? "PENDING" : "SKIP";
                mScorecard.add(row);
            }
        }

        mWorkerEnabled = true;
        mIsRunning = true;
        setButtonsEnabled(true);

        mWorkerThread = new Thread(
                () -> runDiagnosticSuite(forceNsOff, run4PhaseAec, runStandby),
                "VoiceCommAecWorker");
        mWorkerThread.start();

        mPoller = new ReportPoller();
        mPoller.start();
    }

    private void stopTest() {
        mWorkerEnabled = false;
        if (mWorkerThread != null) {
            mWorkerThread.interrupt();
            try {
                mWorkerThread.join(1500);
            } catch (InterruptedException ignored) {
            }
            mWorkerThread = null;
        }
        mIsRunning = false;
        stopPoller();
        keepScreenOn(false);
        updateReportUI();
        setButtonsEnabled(false);
    }

    private void setButtonsEnabled(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
        mShareButton.setEnabled(!running);
        mPlayOffButton.setEnabled(!running && mLastRecordingAecOffLen > 0);
        mPlayOnButton.setEnabled(!running && mLastRecordingAecOnLen > 0);
        mNativeApiCheckBox.setEnabled(!running);
        mJavaApiCheckBox.setEnabled(!running);
        mForceNsOffCheckBox.setEnabled(!running);
        mPhaseAecCheckBox.setEnabled(!running);
        mPhaseStandbyCheckBox.setEnabled(!running);
    }

    private void runDiagnosticSuite(
            boolean forceNsOff, boolean run4PhaseAec, boolean runStandby) {
        AudioManager am = getSystemService(AudioManager.class);
        int originalMode = (am != null) ? am.getMode() : AudioManager.MODE_NORMAL;
        int originalVoiceVol = -1;
        AudioDeviceInfo speakerDevice = null;

        try {
            setProgress("Decoding 48 kHz reference speech stimulus...");
            short[] speechPcm = ensureSpeechPcmDecoded();
            if (speechPcm == null || speechPcm.length == 0) {
                appendTelemetry("ERROR: Failed to prepare 48 kHz speech stimulus.");
                recordFail();
                return;
            }

            if (am != null) {
                setProgress("Configuring MODE_IN_COMMUNICATION & speakerphone routing...");
                am.setMode(AudioManager.MODE_IN_COMMUNICATION);
                for (AudioDeviceInfo dev : am.getAvailableCommunicationDevices()) {
                    if (dev.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
                        speakerDevice = dev;
                        break;
                    }
                }
                if (speakerDevice != null) {
                    am.setCommunicationDevice(speakerDevice);
                    long deadline = SystemClock.elapsedRealtime() + 300;
                    while (SystemClock.elapsedRealtime() < deadline && mWorkerEnabled) {
                        AudioDeviceInfo active = am.getCommunicationDevice();
                        if (active != null
                                && active.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
                            break;
                        }
                        SystemClock.sleep(25);
                    }
                    SystemClock.sleep(50);
                } else {
                    am.setSpeakerphoneOn(true);
                }

                if (!am.isVolumeFixed()) {
                    originalVoiceVol = am.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
                    int maxVol = am.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL);
                    for (int attempt = 0; attempt < 4; attempt++) {
                        am.setStreamVolume(AudioManager.STREAM_VOICE_CALL, maxVol, 0);
                        if (am.getStreamVolume(AudioManager.STREAM_VOICE_CALL) == maxVol) {
                            break;
                        }
                        SystemClock.sleep(40);
                    }
                }
            }

            List<ApiRunMetrics> rowsCopy;
            synchronized (mReportLock) {
                rowsCopy = new ArrayList<>(mScorecard);
            }

            for (int i = 0; i < rowsCopy.size(); i++) {
                if (!mWorkerEnabled) break;
                if (i > 0) {
                    // Allow HAL communication route/DSP teardown to settle between API runs
                    sleepInterruptible(300);
                }
                runSingleApiPath(
                        rowsCopy.get(i), speechPcm, am, forceNsOff, run4PhaseAec, runStandby);
            }

            if (mWorkerEnabled && rowsCopy.size() == 2) {
                evaluateCrossApiComparison(rowsCopy.get(0), rowsCopy.get(1));
            }
        } catch (Exception e) {
            Log.e(TAG, "VoIP & AEC diagnostic failed with exception", e);
            appendTelemetry("EXCEPTION: " + e.getMessage());
            recordFail();
        } finally {
            if (am != null) {
                try {
                    if (originalVoiceVol >= 0 && !am.isVolumeFixed()) {
                        am.setStreamVolume(AudioManager.STREAM_VOICE_CALL, originalVoiceVol, 0);
                    }
                    if (speakerDevice != null) {
                        am.clearCommunicationDevice();
                    }
                    am.setMode(originalMode);
                } catch (Exception e) {
                    Log.w(TAG, "Error restoring AudioManager state", e);
                }
            }
            synchronized (mReportLock) {
                mSuiteCompleted = true;
                if (!mWorkerEnabled) {
                    mSuiteStoppedByUser = true;
                    mCurrentProgress = "Stopped by user.";
                } else {
                    mCurrentProgress = "Complete.";
                }
            }
            mIsRunning = false;
        }
    }

    private void runSingleApiPath(
            ApiRunMetrics row,
            short[] speechPcm,
            AudioManager am,
            boolean forceNsOff,
            boolean run4PhaseAec,
            boolean runStandby) {
        final boolean isNative = row.isNative;
        appendTelemetry("------------------------------------------------");
        appendTelemetry("STREAM: " + row.apiLabel);
        appendTelemetry("------------------------------------------------");

        VoipPlayer player = null;
        long nativeRec = 0;
        AudioRecord javaRec = null;
        AcousticEchoCanceler aec = null;
        NoiseSuppressor ns = null;

        try {
            if (run4PhaseAec) {
                // =====================================================================
                // Phase 1: Ambient Noise Floor (VOICE_RECOGNITION, Silence)
                // =====================================================================
                updatePhaseStatus(row, 1, "RUNNING...");
                setProgress(row.apiLabel + " - Phase 1/5: Ambient Noise Floor (Silence)...");
                appendTelemetry("  [Phase 1] Ambient Noise Floor (VOICE_RECOGNITION, Silence)");

                double p1RmsSum = 0.0;
                int p1Count = 0;

                if (isNative) {
                    nativeRec = nativeOpenInputStream(
                            SAMPLE_RATE, 1, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION);
                    if (nativeRec == 0) {
                        updatePhaseStatus(row, 1, "FAIL (open failed)");
                        appendTelemetry("    FAIL: nativeOpenInputStream(VOICE_RECOGNITION) failed");
                        recordFail();
                        return;
                    }
                    row.rawInMmap = nativeIsMMap(nativeRec);
                    int burst = Math.max(256, nativeGetBurstFrames(nativeRec));
                    nativeStartStream(nativeRec);

                    short[] buf = new short[burst];
                    long end = SystemClock.elapsedRealtime() + PHASE1_SILENCE_MS;
                    while (SystemClock.elapsedRealtime() < end && mWorkerEnabled) {
                        int read = nativeReadStream(nativeRec, buf, 0, burst, 100_000_000L);
                        if (read > 0) {
                            p1RmsSum += computeBlockRms(buf, read);
                            p1Count++;
                        }
                    }
                    nativeStopStream(nativeRec);
                    nativeCloseStream(nativeRec);
                    nativeRec = 0;
                } else {
                    int minBuf = Math.max(
                            4096,
                            AudioRecord.getMinBufferSize(
                                    SAMPLE_RATE,
                                    AudioFormat.CHANNEL_IN_MONO,
                                    AudioFormat.ENCODING_PCM_16BIT) * 2);
                    javaRec = new AudioRecord.Builder()
                            .setAudioSource(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                            .setAudioFormat(new AudioFormat.Builder()
                                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                    .setSampleRate(SAMPLE_RATE)
                                    .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                                    .build())
                            .setBufferSizeInBytes(minBuf)
                            .build();
                    javaRec.startRecording();
                    short[] buf = new short[1024];
                    long end = SystemClock.elapsedRealtime() + PHASE1_SILENCE_MS;
                    while (SystemClock.elapsedRealtime() < end && mWorkerEnabled) {
                        int read = javaRec.read(buf, 0, buf.length, AudioRecord.READ_BLOCKING);
                        if (read > 0) {
                            p1RmsSum += computeBlockRms(buf, read);
                            p1Count++;
                        }
                    }
                    javaRec.stop();
                    javaRec.release();
                    javaRec = null;
                }

                if (!mWorkerEnabled) return;

                double avgNoiseRms = (p1Count > 0) ? (p1RmsSum / p1Count) : MIN_RMS_VAL;
                row.noiseFloorDbfs = 20.0 * Math.log10(Math.max(avgNoiseRms, MIN_RMS_VAL));
                boolean p1Pass = (row.noiseFloorDbfs <= THRESHOLD_AMBIENT_NOISE_CEILING_DBFS);
                updatePhaseStatus(
                        row,
                        1,
                        String.format(
                                Locale.US,
                                "%s (%.1f dBFS)",
                                p1Pass ? "PASS" : "FAIL (noisy room)",
                                row.noiseFloorDbfs));
                appendTelemetry(String.format(
                        Locale.US,
                        "    Noise Floor : %.1f dBFS (ceiling <= %.1f dBFS) -> %s",
                        row.noiseFloorDbfs,
                        THRESHOLD_AMBIENT_NOISE_CEILING_DBFS,
                        p1Pass ? "PASS" : "FAIL"));
                if (p1Pass) {
                    recordPass();
                } else {
                    recordFail();
                    Finding f = new Finding();
                    f.id = "AMBIENT-NOISE-TOO-HIGH";
                    f.severity = "FAIL";
                    f.title = "Ambient room noise floor exceeds -35.0 dBFS ceiling";
                    f.whatHappened = "During Phase 1 silence capture on VOICE_RECOGNITION, the microphone recorded higher background noise than allowed for reliable echo cancellation measurement.";
                    f.androidContract = "Acoustic echo cancellation diagnostics require a quiet room (ambient noise <= -35.0 dBFS) so residual echo can be separated from room noise.";
                    f.whyItMatters = "High ambient noise masks echo attenuation (ERLE) and inflates microphone envelope variance.";
                    f.howToFix = "Re-run the test in a quiet room away from fans, HVAC vents, or conversation, and verify that the microphone hardware bias/ADC self-noise is below -40 dBFS.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Phase 1 ambient noise = %.1f dBFS (limit <= %.1f dBFS).",
                                    row.apiLabel,
                                    row.noiseFloorDbfs,
                                    THRESHOLD_AMBIENT_NOISE_CEILING_DBFS));
                }

                if (!sleepInterruptible(300)) return;

                // =====================================================================
                // Phase 2: Raw Acoustic Coupling & Baseline RTL (VOICE_RECOGNITION)
                // =====================================================================
                updatePhaseStatus(row, 2, "RUNNING...");
                setProgress(row.apiLabel + " - Phase 2/5: Raw Coupling & Baseline RTL...");
                appendTelemetry("  [Phase 2] Raw Coupling & Baseline RTL (VOICE_RECOGNITION)");

                player = new VoipPlayer(isNative, speechPcm);
                player.setPlaySpeech(false);
                if (!player.start()) {
                    updatePhaseStatus(row, 2, "FAIL (output open)");
                    appendTelemetry("    FAIL: Could not start VoIP output player");
                    recordFail();
                    return;
                }

                if (isNative && am != null && !am.isVolumeFixed()) {
                    int maxVol = am.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL);
                    if (maxVol > 1) {
                        am.setStreamVolume(AudioManager.STREAM_VOICE_CALL, maxVol - 1, 0);
                        am.setStreamVolume(AudioManager.STREAM_VOICE_CALL, maxVol, 0);
                    }
                }

                int burst2 = 960;
                if (isNative) {
                    nativeRec = nativeOpenInputStream(
                            SAMPLE_RATE, 1, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION);
                    if (nativeRec == 0) {
                        updatePhaseStatus(row, 2, "FAIL (input open)");
                        appendTelemetry("    FAIL: nativeOpenInputStream failed in Phase 2");
                        recordFail();
                        return;
                    }
                    row.rawOutMmap = nativeIsMMap(player.getNativeHandle());
                    row.rawInMmap = nativeIsMMap(nativeRec);
                    row.outBurstFrames = nativeGetBurstFrames(player.getNativeHandle());
                    row.outCapacityFrames = nativeGetBufferCapacityFrames(player.getNativeHandle());
                    burst2 = Math.max(256, nativeGetBurstFrames(nativeRec));
                    nativeStartStream(nativeRec);
                } else {
                    int minBuf = Math.max(
                            4096,
                            AudioRecord.getMinBufferSize(
                                    SAMPLE_RATE,
                                    AudioFormat.CHANNEL_IN_MONO,
                                    AudioFormat.ENCODING_PCM_16BIT) * 2);
                    javaRec = new AudioRecord.Builder()
                            .setAudioSource(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                            .setAudioFormat(new AudioFormat.Builder()
                                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                    .setSampleRate(SAMPLE_RATE)
                                    .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                                    .build())
                            .setBufferSizeInBytes(minBuf)
                            .build();
                    javaRec.startRecording();
                }

                // Stabilize DMA for 200 ms while playing silence
                short[] ioBuf2 = new short[burst2];
                long javaFramesRead2 = 0;
                long stabEnd2 = SystemClock.elapsedRealtime() + 200;
                while (SystemClock.elapsedRealtime() < stabEnd2 && mWorkerEnabled) {
                    if (isNative) {
                        nativeReadStream(nativeRec, ioBuf2, 0, burst2, 100_000_000L);
                    } else {
                        int r = javaRec.read(ioBuf2, 0, burst2, AudioRecord.READ_BLOCKING);
                        if (r > 0) javaFramesRead2 += r;
                    }
                }

                int maxSamples = (SAMPLE_RATE * PHASE_SPEECH_MS) / 1000;
                short[] rawCapture = new short[maxSamples];
                int rawCaptureLen = 0;
                double p2RmsSum = 0.0;
                int p2Count = 0;

                player.playSpeechFromStart();
                long end2 = SystemClock.elapsedRealtime() + PHASE_SPEECH_MS;
                while (SystemClock.elapsedRealtime() < end2
                        && rawCaptureLen < rawCapture.length
                        && mWorkerEnabled) {
                    int read = isNative
                            ? nativeReadStream(nativeRec, ioBuf2, 0, burst2, 100_000_000L)
                            : javaRec.read(ioBuf2, 0, burst2, AudioRecord.READ_BLOCKING);
                    if (!isNative && read > 0) {
                        javaFramesRead2 += read;
                    }
                    if (read > 0) {
                        int toCopy = Math.min(read, rawCapture.length - rawCaptureLen);
                        System.arraycopy(ioBuf2, 0, rawCapture, rawCaptureLen, toCopy);
                        rawCaptureLen += toCopy;
                        p2RmsSum += computeBlockRms(ioBuf2, read);
                        p2Count++;
                    }
                }

                if (isNative) {
                    row.rawTimestampRtlMs = nativeGetRoundTripTimestampLatencyMillis(
                            player.getNativeHandle(), nativeRec);
                    nativeStopStream(nativeRec);
                    nativeCloseStream(nativeRec);
                    nativeRec = 0;
                } else {
                    row.rawTimestampRtlMs = computeJavaTimestampLatencyMillis(
                            player, javaRec, javaFramesRead2);
                    javaRec.stop();
                    javaRec.release();
                    javaRec = null;
                }
                player.setPlaySpeech(false);

                if (!mWorkerEnabled) return;

                double avgRawRms = (p2Count > 0) ? (p2RmsSum / p2Count) : MIN_RMS_VAL;
                row.rawCouplingDbfs = 20.0 * Math.log10(Math.max(avgRawRms, MIN_RMS_VAL));
                row.stimulusSnrDb = row.rawCouplingDbfs - row.noiseFloorDbfs;
                if (rawCaptureLen > 0) {
                    short[] slice = Arrays.copyOf(rawCapture, rawCaptureLen);
                    row.rawAcousticRtlMs = computeAcousticLatencyMs(
                            speechPcm, mSpeechSampleRate, slice, SAMPLE_RATE, 500);
                }

                boolean p2Pass = (row.stimulusSnrDb >= THRESHOLD_STIMULUS_SNR_DB);
                updatePhaseStatus(
                        row,
                        2,
                        String.format(
                                Locale.US,
                                "%s (SNR %.1fdB, RTL %.1fms)",
                                p2Pass ? "PASS" : "FAIL",
                                row.stimulusSnrDb,
                                row.rawAcousticRtlMs));
                appendTelemetry(String.format(
                        Locale.US,
                        "    Raw Level   : %.1f dBFS (SNR = %.1f dB, min >= %.1f dB)",
                        row.rawCouplingDbfs,
                        row.stimulusSnrDb,
                        THRESHOLD_STIMULUS_SNR_DB));
                appendTelemetry(String.format(
                        Locale.US,
                        "    Raw RTL     : Acoustic = %.1f ms | HW TS = %s",
                        row.rawAcousticRtlMs,
                        formatMs(row.rawTimestampRtlMs)));
                if (p2Pass) {
                    recordPass();
                } else {
                    recordFail();
                    Finding f = new Finding();
                    f.id = "STIMULUS-SNR-TOO-LOW";
                    f.severity = "FAIL";
                    f.title = "Speaker-to-microphone acoustic coupling SNR is below 12.0 dB";
                    f.whatHappened = "During Phase 2 speech playback on VOICE_RECOGNITION, the captured loudspeaker level was Less than 12.0 dB above the Phase 1 room noise floor.";
                    f.androidContract = "Built-in speakerphone playback at maximum STREAM_VOICE_CALL volume must produce at least 12.0 dB SNR at the built-in microphone.";
                    f.whyItMatters = "Insufficient loudspeaker coupling prevents accurate measurement of Echo Return Loss Enhancement (ERLE).";
                    f.howToFix = "Verify that STREAM_VOICE_CALL speakerphone routing and volume curves are active, nothing is obstructing the speaker or microphone ports, and MMAP playback volume attenuation is properly applied.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Raw coupling = %.1f dBFS vs. Noise floor = %.1f dBFS (SNR = %.1f dB < 12.0 dB).",
                                    row.apiLabel,
                                    row.rawCouplingDbfs,
                                    row.noiseFloorDbfs,
                                    row.stimulusSnrDb));
                }

                if (!sleepInterruptible(500)) return;

                // =====================================================================
                // Phase 3: VoIP Path with AEC OFF (VOICE_COMMUNICATION, Bypass & RTL)
                // =====================================================================
                updatePhaseStatus(row, 3, "RUNNING...");
                setProgress(row.apiLabel + " - Phase 3/5: VoIP Bypass (AEC OFF) & RTL...");
                appendTelemetry("  [Phase 3] VoIP Bypass & RTL (VOICE_COMMUNICATION, AEC OFF)");

                int sessionId = -1;
                int burst3 = 960;
                if (isNative) {
                    nativeRec = nativeOpenInputStream(
                            SAMPLE_RATE, 1, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
                    if (nativeRec == 0) {
                        updatePhaseStatus(row, 3, "FAIL (open failed)");
                        appendTelemetry("    FAIL: nativeOpenInputStream(VOICE_COMMUNICATION) failed");
                        recordFail();
                        return;
                    }
                    row.voipOutMmap = nativeIsMMap(player.getNativeHandle());
                    row.voipInMmap = nativeIsMMap(nativeRec);
                    row.voipOutExclusive = nativeIsExclusive(player.getNativeHandle());
                    row.voipInExclusive = nativeIsExclusive(nativeRec);
                    row.voipInBurstFrames = nativeGetBurstFrames(nativeRec);
                    row.voipInCapacityFrames = nativeGetBufferCapacityFrames(nativeRec);
                    row.configSummary = String.format(
                            Locale.US,
                            "MMAP Out=%s In=%s, %df/%df",
                            row.voipOutMmap ? "YES" : "NO",
                            row.voipInMmap ? "YES" : "NO",
                            row.outBurstFrames,
                            row.voipInBurstFrames);
                    sessionId = nativeGetSessionId(nativeRec);
                    burst3 = Math.max(256, row.voipInBurstFrames);
                    nativeStartStream(nativeRec);
                } else {
                    int minBuf = Math.max(
                            4096,
                            AudioRecord.getMinBufferSize(
                                    SAMPLE_RATE,
                                    AudioFormat.CHANNEL_IN_MONO,
                                    AudioFormat.ENCODING_PCM_16BIT) * 2);
                    javaRec = new AudioRecord.Builder()
                            .setAudioSource(MediaRecorder.AudioSource.VOICE_COMMUNICATION)
                            .setAudioFormat(new AudioFormat.Builder()
                                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                    .setSampleRate(SAMPLE_RATE)
                                    .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                                    .build())
                            .setBufferSizeInBytes(minBuf)
                            .build();
                    javaRec.startRecording();
                    sessionId = javaRec.getAudioSessionId();
                    row.configSummary = "AudioTrack + AudioRecord VOIP";
                }

                if (mAecSupported && sessionId > 0) {
                    try {
                        aec = AcousticEchoCanceler.create(sessionId);
                        if (aec != null) {
                            aec.setEnabled(false);
                        }
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to create AcousticEchoCanceler", e);
                    }
                }
                if (mNsSupported && sessionId > 0) {
                    try {
                        ns = NoiseSuppressor.create(sessionId);
                        if (ns != null && forceNsOff) {
                            ns.setEnabled(false);
                        }
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to create NoiseSuppressor", e);
                    }
                }

                int maxShots = (PHASE_SPEECH_MS / SHOT_INTERVAL_MS) + 5;
                RmsEnvelopeTracker playerEnv3 = new RmsEnvelopeTracker(SAMPLE_RATE, maxShots);
                RmsEnvelopeTracker recEnv3 = new RmsEnvelopeTracker(SAMPLE_RATE, maxShots);
                player.setRmsTracker(playerEnv3);
                player.setPlaySpeech(false);

                short[] ioBuf3 = new short[burst3];
                long javaFramesReadComm = 0;
                long flushEnd3 = SystemClock.elapsedRealtime() + 350;
                while (SystemClock.elapsedRealtime() < flushEnd3 && mWorkerEnabled) {
                    if (isNative) {
                        nativeReadStream(nativeRec, ioBuf3, 0, burst3, 100_000_000L);
                    } else {
                        int r = javaRec.read(ioBuf3, 0, burst3, AudioRecord.READ_BLOCKING);
                        if (r > 0) javaFramesReadComm += r;
                    }
                }

                playerEnv3.reset();
                recEnv3.reset();
                playerEnv3.setRunning(true);
                recEnv3.setRunning(true);
                player.playSpeechFromStart();

                short[] recAecOff = new short[maxSamples];
                int recAecOffLen = 0;
                double p3RmsSum = 0.0;
                int p3Count = 0;
                long end3 = SystemClock.elapsedRealtime() + PHASE_SPEECH_MS;
                long nextShot3 = SystemClock.elapsedRealtime() + SHOT_INTERVAL_MS;

                while (SystemClock.elapsedRealtime() < end3 && mWorkerEnabled) {
                    int read = isNative
                            ? nativeReadStream(nativeRec, ioBuf3, 0, burst3, 100_000_000L)
                            : javaRec.read(ioBuf3, 0, burst3, AudioRecord.READ_BLOCKING);
                    if (!isNative && read > 0) {
                        javaFramesReadComm += read;
                    }
                    if (read > 0) {
                        int toCopy = Math.min(read, recAecOff.length - recAecOffLen);
                        if (toCopy > 0) {
                            System.arraycopy(ioBuf3, 0, recAecOff, recAecOffLen, toCopy);
                            recAecOffLen += toCopy;
                        }
                        recEnv3.updateDirect(ioBuf3, read);
                        p3RmsSum += computeBlockRms(ioBuf3, read);
                        p3Count++;
                    }
                    if (SystemClock.elapsedRealtime() >= nextShot3) {
                        recEnv3.captureShot();
                        playerEnv3.captureShot();
                        nextShot3 += SHOT_INTERVAL_MS;
                        setProgress(String.format(
                                Locale.US,
                                "%s - Phase 3/5 (AEC OFF): Rec %.1f dBFS | Play %.1f dBFS",
                                row.apiLabel,
                                recEnv3.getCurrentDbfs(),
                                playerEnv3.getCurrentDbfs()));
                    }
                }

                if (isNative) {
                    row.voipOffTimestampRtlMs = nativeGetRoundTripTimestampLatencyMillis(
                            player.getNativeHandle(), nativeRec);
                } else {
                    row.voipOffTimestampRtlMs = computeJavaTimestampLatencyMillis(
                            player, javaRec, javaFramesReadComm);
                }

                player.setPlaySpeech(false);
                player.setRmsTracker(null);
                playerEnv3.setRunning(false);
                recEnv3.setRunning(false);

                mLastRecordingAecOff = recAecOff;
                mLastRecordingAecOffLen = recAecOffLen;
                mLastRecordingApiLabel = row.apiLabel;

                if (!mWorkerEnabled) return;

                double avgOffRms = (p3Count > 0) ? (p3RmsSum / p3Count) : MIN_RMS_VAL;
                row.aecOffDbfs = 20.0 * Math.log10(Math.max(avgOffRms, MIN_RMS_VAL));
                row.signedGainDeltaDb = row.aecOffDbfs - row.rawCouplingDbfs;
                row.bypassDeltaDb = Math.abs(row.signedGainDeltaDb);

                // Scale VOICE_COMMUNICATION levels by the measured VOICE_RECOGNITION /
                // VOICE_COMMUNICATION preset gain ratio so differences in nominal volume
                // between presets are normalized before floor clamping and ERLE scoring.
                double rawToVoipRatio = (avgOffRms > 1e-6) ? (avgRawRms / avgOffRms) : 1.0;
                double voipGainScale = Math.max(0.05, Math.min(20.0, rawToVoipRatio));
                row.voipGainScaleDb = 20.0 * Math.log10(voipGainScale);
                row.scaledAecOffDbfs = 20.0 * Math.log10(
                        Math.max(avgOffRms * voipGainScale, MIN_RMS_VAL));
                double corrBroadband = Math.max(
                        computeEnvelopeCorrelation(
                                playerEnv3.getSnapshotsCopy(), recEnv3.getSnapshotsCopy(), 1.0),
                        computeEnvelopeCorrelation(
                                playerEnv3.getSnapshotsCopy(),
                                recEnv3.getSnapshotsCopy(),
                                voipGainScale));
                double corrPreemph = Math.max(
                        computeEnvelopeCorrelation(
                                playerEnv3.getPreemphSnapshotsCopy(),
                                recEnv3.getSnapshotsCopy(),
                                1.0),
                        computeEnvelopeCorrelation(
                                playerEnv3.getPreemphSnapshotsCopy(),
                                recEnv3.getSnapshotsCopy(),
                                voipGainScale));
                row.corrAecOff = Math.max(corrBroadband, corrPreemph);

                if (recAecOffLen > 0) {
                    short[] slice = Arrays.copyOf(recAecOff, recAecOffLen);
                    row.voipOffAcousticRtlMs = computeAcousticLatencyMs(
                            speechPcm, mSpeechSampleRate, slice, SAMPLE_RATE, 500);
                }

                boolean corrOffOk = (row.corrAecOff >= THRESHOLD_CORR_AEC_OFF_MIN);
                if (corrOffOk) {
                    updatePhaseStatus(
                            row,
                            3,
                            String.format(
                                    Locale.US,
                                    "PASS (C=%.2f, scale=%+.1fdB, %.0fms)",
                                    row.corrAecOff,
                                    row.voipGainScaleDb,
                                    row.voipOffAcousticRtlMs));
                    recordPass();
                } else {
                    voipGainScale = 1.0;
                    row.voipGainScaleDb = 0.0;
                    row.scaledAecOffDbfs = row.aecOffDbfs;
                    updatePhaseStatus(
                            row,
                            3,
                            String.format(
                                    Locale.US,
                                    "FAIL (C=%.2f<0.60, dG=%+.1fdB)",
                                    row.corrAecOff,
                                    row.signedGainDeltaDb));
                    recordFail();
                    Finding f = new Finding();
                    f.id = "AEC-BYPASS-IGNORED-OR-ALWAYS-ON";
                    f.severity = "FAIL";
                    f.title = "AcousticEchoCanceler.setEnabled(false) failed to bypass echo cancellation";
                    f.whatHappened = "During Phase 3 on VOICE_COMMUNICATION with AcousticEchoCanceler.setEnabled(false), the loudspeaker speech envelope was still suppressed (Correlation < 0.60).";
                    f.androidContract = "CDD 5.4 / AcousticEchoCanceler API: Disabling AcousticEchoCanceler via setEnabled(false) on a VOICE_COMMUNICATION stream must disable echo cancellation in the HAL/DSP.";
                    f.whyItMatters = "Apps and compliance tests cannot disable echo cancellation when needed, or the HAL is running unreported always-on DSP AEC.";
                    f.howToFix = "Ensure the Audio HAL effect implementation forwards setEnabled(false) to the voice DSP module and bypasses AEC adaptation/subtraction when disabled.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Phase 3 AEC OFF Corr = %.2f (< 0.60), level = %.1f dBFS (delta vs raw = %+.1f dB).",
                                    row.apiLabel,
                                    row.corrAecOff,
                                    row.aecOffDbfs,
                                    row.signedGainDeltaDb));
                }

                appendTelemetry(String.format(
                        Locale.US,
                        "    AEC OFF Lvl : %.1f dBFS -> Scaled %.1f dBFS (Scale = %+.1f dB, Corr = %.2f)",
                        row.aecOffDbfs,
                        row.scaledAecOffDbfs,
                        row.voipGainScaleDb,
                        row.corrAecOff));
                appendTelemetry(String.format(
                        Locale.US,
                        "    VoIP RTL    : Acoustic = %.1f ms | HW TS = %s",
                        row.voipOffAcousticRtlMs,
                        formatMs(row.voipOffTimestampRtlMs)));

                // Check MMAP fallback on Native VoIP path (Android 18+ supports MMAP + sessionId AEC)
                if (isNative && Build.VERSION.SDK_INT >= 38
                        && (mMmapSupported || row.rawOutMmap || row.rawInMmap)
                        && (!row.voipOutMmap || !row.voipInMmap)) {
                    recordWarn();
                    Finding f = new Finding();
                    f.id = "VOIP-MMAP-FALLBACK";
                    f.severity = "WARNING";
                    f.title = "Native AAudio VOICE_COMMUNICATION dropped out of MMAP mode";
                    f.whatHappened = "The device supports AAudio MMAP, but opening a LowLatency stream with USAGE_VOICE_COMMUNICATION or INPUT_PRESET_VOICE_COMMUNICATION fell back to a non-MMAP legacy stream.";
                    f.androidContract = "On Android 18+, low-latency VoIP and voice agent streams using AAudio PerformanceMode::LowLatency should stay on the MMAP path when hardware DSP AEC is supported.";
                    f.whyItMatters = "Falling back from MMAP to legacy AudioFlinger mixer/capture threads adds 20-60 ms of extra round-trip latency during voice communication.";
                    f.howToFix = "Configure the Audio HAL and audio_policy_configuration.xml so USAGE_VOICE_COMMUNICATION output and INPUT_PRESET_VOICE_COMMUNICATION input profiles support MMAP (MmapPlaybackThread / MmapCaptureThread) with hardware DSP AEC bound by sessionId.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Device MMAP=%s, Raw MMAP (Out=%s, In=%s) -> VoIP MMAP (Out=%s, In=%s).",
                                    row.apiLabel,
                                    mMmapSupported ? "YES" : "NO",
                                    row.rawOutMmap ? "YES" : "NO",
                                    row.rawInMmap ? "YES" : "NO",
                                    row.voipOutMmap ? "YES" : "NO",
                                    row.voipInMmap ? "YES" : "NO"));
                }

                // =====================================================================
                // Phase 4: VoIP Path with AEC ON (ERLE & Residual Correlation)
                // =====================================================================
                updatePhaseStatus(row, 4, "RUNNING...");
                setProgress(row.apiLabel + " - Phase 4/5: Active AEC (ERLE & Correlation)...");
                appendTelemetry("  [Phase 4] Active AEC & ERLE (VOICE_COMMUNICATION, AEC ON)");

                if (aec != null) {
                    aec.setEnabled(true);
                }

                RmsEnvelopeTracker playerEnv4 = new RmsEnvelopeTracker(SAMPLE_RATE, maxShots);
                RmsEnvelopeTracker recEnv4 = new RmsEnvelopeTracker(SAMPLE_RATE, maxShots);
                recEnv4.setGainScale(voipGainScale);
                player.setRmsTracker(playerEnv4);
                player.setPlaySpeech(false);

                long flushEnd4 = SystemClock.elapsedRealtime() + 300;
                while (SystemClock.elapsedRealtime() < flushEnd4 && mWorkerEnabled) {
                    if (isNative) {
                        nativeReadStream(nativeRec, ioBuf3, 0, burst3, 100_000_000L);
                    } else {
                        int r = javaRec.read(ioBuf3, 0, burst3, AudioRecord.READ_BLOCKING);
                        if (r > 0) javaFramesReadComm += r;
                    }
                }

                playerEnv4.reset();
                recEnv4.reset();
                recEnv4.setGainScale(voipGainScale);
                playerEnv4.setRunning(true);
                recEnv4.setRunning(true);
                player.playSpeechFromStart();

                short[] recAecOn = new short[maxSamples];
                int recAecOnLen = 0;
                double p4RmsSum = 0.0;
                double p4ScaledRmsSum = 0.0;
                int p4Count = 0;
                long end4 = SystemClock.elapsedRealtime() + PHASE_SPEECH_MS;
                long nextShot4 = SystemClock.elapsedRealtime() + SHOT_INTERVAL_MS;

                while (SystemClock.elapsedRealtime() < end4 && mWorkerEnabled) {
                    int read = isNative
                            ? nativeReadStream(nativeRec, ioBuf3, 0, burst3, 100_000_000L)
                            : javaRec.read(ioBuf3, 0, burst3, AudioRecord.READ_BLOCKING);
                    if (!isNative && read > 0) {
                        javaFramesReadComm += read;
                    }
                    if (read > 0) {
                        int toCopy = Math.min(read, recAecOn.length - recAecOnLen);
                        if (toCopy > 0) {
                            System.arraycopy(ioBuf3, 0, recAecOn, recAecOnLen, toCopy);
                            recAecOnLen += toCopy;
                        }
                        recEnv4.updateDirect(ioBuf3, read);
                        p4RmsSum += computeBlockRms(ioBuf3, read);
                        p4ScaledRmsSum += computeBlockRmsScaled(ioBuf3, read, voipGainScale);
                        p4Count++;
                    }
                    if (SystemClock.elapsedRealtime() >= nextShot4) {
                        recEnv4.captureShot();
                        playerEnv4.captureShot();
                        nextShot4 += SHOT_INTERVAL_MS;
                        setProgress(String.format(
                                Locale.US,
                                "%s - Phase 4/5 (AEC ON): Rec %.1f dBFS | Play %.1f dBFS",
                                row.apiLabel,
                                recEnv4.getCurrentDbfs(),
                                playerEnv4.getCurrentDbfs()));
                    }
                }

                if (isNative) {
                    row.voipOnTimestampRtlMs = nativeGetRoundTripTimestampLatencyMillis(
                            player.getNativeHandle(), nativeRec);
                } else {
                    row.voipOnTimestampRtlMs = computeJavaTimestampLatencyMillis(
                            player, javaRec, javaFramesReadComm);
                }

                player.setPlaySpeech(false);
                player.setRmsTracker(null);
                playerEnv4.setRunning(false);
                recEnv4.setRunning(false);

                mLastRecordingAecOn = recAecOn;
                mLastRecordingAecOnLen = recAecOnLen;

                if (!mWorkerEnabled) return;

                double avgOnRms = (p4Count > 0) ? (p4RmsSum / p4Count) : MIN_RMS_VAL;
                double avgOnScaledRms = (p4Count > 0) ? (p4ScaledRmsSum / p4Count) : MIN_RMS_VAL;
                row.aecOnDbfs = 20.0 * Math.log10(Math.max(avgOnRms, MIN_RMS_VAL));
                row.scaledAecOnDbfs = 20.0 * Math.log10(Math.max(avgOnScaledRms, MIN_RMS_VAL));
                row.rawErleDb = row.aecOffDbfs - row.aecOnDbfs;
                row.selfErleDb = row.scaledAecOffDbfs - row.scaledAecOnDbfs;
                row.peakRefErleDb = Math.max(row.rawCouplingDbfs, row.scaledAecOffDbfs)
                        - row.scaledAecOnDbfs;
                row.distToFloorDb = row.scaledAecOnDbfs - row.noiseFloorDbfs;
                double p4CorrFloorDb = Math.max(MIN_RMS_DB, row.scaledAecOffDbfs - 18.0);
                row.corrAecOn = Math.max(
                        computeEnvelopeCorrelation(
                                playerEnv4.getSnapshotsCopy(),
                                recEnv4.getSnapshotsCopy(),
                                1.0,
                                p4CorrFloorDb),
                        computeEnvelopeCorrelation(
                                playerEnv4.getPreemphSnapshotsCopy(),
                                recEnv4.getSnapshotsCopy(),
                                1.0,
                                p4CorrFloorDb));
                row.metricsPopulated = true;

                boolean erleOk = (row.peakRefErleDb >= THRESHOLD_ERLE_DB)
                        || (row.selfErleDb >= THRESHOLD_ERLE_DB)
                        || (row.distToFloorDb <= THRESHOLD_DIST_TO_FLOOR_DB
                                && row.peakRefErleDb >= THRESHOLD_MIN_ERLE_AT_FLOOR_DB);
                boolean corrOnOk = (row.corrAecOn <= THRESHOLD_CORR_AEC_ON_MAX);
                boolean p4Pass = erleOk && corrOnOk;

                updatePhaseStatus(
                        row,
                        4,
                        String.format(
                                Locale.US,
                                "%s (ERLE %.1fdB, C=%.2f, %.0fms)",
                                p4Pass ? "PASS" : "FAIL",
                                row.selfErleDb,
                                row.corrAecOn,
                                row.voipOnTimestampRtlMs));
                appendTelemetry(String.format(
                        Locale.US,
                        "    AEC ON Lvl  : %.1f dBFS -> Scaled %.1f dBFS (Norm-ERLE = %.1f dB, Raw = %.1f dB)",
                        row.aecOnDbfs,
                        row.scaledAecOnDbfs,
                        row.selfErleDb,
                        row.rawErleDb));
                appendTelemetry(String.format(
                        Locale.US,
                        "    Residual    : Dist-to-Floor = %.1f dB | Corr = %.2f (<= %.2f)",
                        row.distToFloorDb,
                        row.corrAecOn,
                        THRESHOLD_CORR_AEC_ON_MAX));
                appendTelemetry(String.format(
                        Locale.US,
                        "    AEC ON RTL  : HW TS = %s -> %s",
                        formatMs(row.voipOnTimestampRtlMs),
                        p4Pass ? "PASS" : "FAIL"));

                if (p4Pass) {
                    recordPass();
                } else {
                    recordFail();
                    Finding f = new Finding();
                    f.id = "AEC-INSUFFICIENT-CANCELLATION";
                    f.severity = "FAIL";
                    f.title = "Active AEC failed ERLE (>= 15.0 dB) or residual envelope correlation (<= 0.50)";
                    f.whatHappened = "With AcousticEchoCanceler.setEnabled(true) on VOICE_COMMUNICATION, loudspeaker speech bled into the captured microphone signal.";
                    f.androidContract = "4-Phase AudioAEC requires ERLE >= 15.0 dB (or Dist-to-Floor <= 6.0 dB with ERLE >= 8.0 dB) and Residual Envelope Correlation <= 0.50 on both Java and Native AAudio VoIP paths.";
                    f.whyItMatters = "Poor echo cancellation causes conversational voice agents to hear and transcribe their own TTS output (self-interruption) and causes echo in VoIP calls.";
                    f.howToFix = "1. Verify that the HAL feeds the speakerphone playback echo_reference into the hardware AEC module for both Legacy and MMAP (USAGE_VOICE_COMMUNICATION) output streams.\n  2. Check for sample-rate or DMA timestamp jumps between output and input streams that prevent the adaptive AEC filter from converging.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Norm-ERLE = %.1f dB (Raw = %.1f dB), Dist-to-Floor = %.1f dB, Corr = %.2f.",
                                    row.apiLabel,
                                    row.selfErleDb,
                                    row.rawErleDb,
                                    row.distToFloorDb,
                                    row.corrAecOn));
                }

                // Check 100ms Voice Agent Round-Trip Latency target on Native AAudio
                double bestVoipRtlMs = (row.voipOnTimestampRtlMs > 0)
                        ? row.voipOnTimestampRtlMs
                        : row.voipOffAcousticRtlMs;
                if (isNative && bestVoipRtlMs > THRESHOLD_VOICE_AGENT_RTL_MS) {
                    recordWarn();
                    Finding f = new Finding();
                    f.id = "VOIP-RTL-EXCEEDS-100MS-TARGET";
                    f.severity = "WARNING";
                    f.title = "Native AAudio VoIP Round-Trip Latency exceeds 100.0 ms target";
                    f.whatHappened = "Measured full-duplex round-trip latency on the Native AAudio VOICE_COMMUNICATION path exceeded the 100.0 ms real-time budget.";
                    f.androidContract = "Low-latency interactive voice agents and VoIP communication target <= 100.0 ms round-trip audio latency.";
                    f.whyItMatters = "High VoIP path latency delays barge-in detection, degrades AEC filter convergence, and adds noticeable conversational lag.";
                    f.howToFix = "Ensure VOICE_COMMUNICATION streams use MMAP with small period/burst sizes (<= 5 ms) and avoid large DSP block-buffering queues in the HAL.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: VoIP Acoustic RTL = %.1f ms, VoIP HW Timestamp RTL = %s (Raw RTL = %.1f ms).",
                                    row.apiLabel,
                                    row.voipOffAcousticRtlMs,
                                    formatMs(row.voipOnTimestampRtlMs),
                                    row.rawAcousticRtlMs));
                }
            }

            // =========================================================================
            // Phase 5: Multi-Turn Voice Agent Standby (>3.5s Pause + Resume)
            // =========================================================================
            if (runStandby && mWorkerEnabled) {
                updatePhaseStatus(row, 5, "RUNNING...");
                setProgress(row.apiLabel + " - Phase 5/5: >3.5s Turn Pause & Resume...");
                appendTelemetry("  [Phase 5] Multi-Turn Standby (>3.5s Pause -> Resume)");

                if (player == null) {
                    player = new VoipPlayer(isNative, speechPcm);
                    player.setPlaySpeech(false);
                    player.start();
                }
                int burst5 = 960;
                if (isNative && nativeRec == 0) {
                    nativeRec = nativeOpenInputStream(
                            SAMPLE_RATE, 1, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
                    if (nativeRec != 0) {
                        burst5 = Math.max(256, nativeGetBurstFrames(nativeRec));
                        int sid = nativeGetSessionId(nativeRec);
                        if (mAecSupported && sid > 0 && aec == null) {
                            aec = AcousticEchoCanceler.create(sid);
                            if (aec != null) aec.setEnabled(true);
                        }
                        nativeStartStream(nativeRec);
                    }
                } else if (!isNative && javaRec == null) {
                    int minBuf = Math.max(
                            4096,
                            AudioRecord.getMinBufferSize(
                                    SAMPLE_RATE,
                                    AudioFormat.CHANNEL_IN_MONO,
                                    AudioFormat.ENCODING_PCM_16BIT) * 2);
                    javaRec = new AudioRecord.Builder()
                            .setAudioSource(MediaRecorder.AudioSource.VOICE_COMMUNICATION)
                            .setAudioFormat(new AudioFormat.Builder()
                                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                    .setSampleRate(SAMPLE_RATE)
                                    .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                                    .build())
                            .setBufferSizeInBytes(minBuf)
                            .build();
                    javaRec.startRecording();
                    int sid = javaRec.getAudioSessionId();
                    if (mAecSupported && sid > 0 && aec == null) {
                        aec = AcousticEchoCanceler.create(sid);
                        if (aec != null) aec.setEnabled(true);
                    }
                } else if (isNative && nativeRec != 0) {
                    burst5 = Math.max(256, nativeGetBurstFrames(nativeRec));
                }

                short[] ioBuf5 = new short[burst5];
                // Warm up for 250 ms before stopping
                long warmEnd = SystemClock.elapsedRealtime() + 250;
                while (SystemClock.elapsedRealtime() < warmEnd && mWorkerEnabled) {
                    if (isNative) {
                        nativeReadStream(nativeRec, ioBuf5, 0, burst5, 20_000_000L);
                    } else {
                        javaRec.read(ioBuf5, 0, burst5, AudioRecord.READ_BLOCKING);
                    }
                }

                if (isNative) {
                    row.standbyPreTsPos = nativeGetTimestampPosition(nativeRec);
                    nativeStopStream(nativeRec);
                    nativeStopStream(player.getNativeHandle());
                } else {
                    AudioTimestamp ts = new AudioTimestamp();
                    if (javaRec.getTimestamp(ts, AudioTimestamp.TIMEBASE_MONOTONIC)
                            == AudioRecord.SUCCESS) {
                        row.standbyPreTsPos = ts.framePosition;
                    }
                    javaRec.stop();
                    player.pauseJavaTrack();
                }

                appendTelemetry(String.format(
                        Locale.US,
                        "    Sleeping %d ms to trigger HAL idle standby (preTs=%d)...",
                        STANDBY_PAUSE_MS,
                        row.standbyPreTsPos));
                if (!sleepInterruptible(STANDBY_PAUSE_MS)) return;

                long resumeStartMs = SystemClock.elapsedRealtime();
                int startInRes = 0;
                int startOutRes = 0;
                if (isNative) {
                    startOutRes = nativeStartStream(player.getNativeHandle());
                    startInRes = nativeStartStream(nativeRec);
                } else {
                    player.resumeJavaTrack();
                    javaRec.startRecording();
                }
                player.playSpeechFromStart();

                long firstReadMs = -1;
                boolean nonZeroVerified = false;
                boolean probedAecOffForLiveness = false;
                boolean aecEnabledAfterWake =
                        !mAecSupported || (aec != null && aec.getEnabled());
                double postRmsSum = 0.0;
                int postCount = 0;
                long resumeEnd = SystemClock.elapsedRealtime() + STANDBY_RESUME_MS;
                long probeSwitchTime = resumeStartMs + (STANDBY_RESUME_MS / 2);
                while (SystemClock.elapsedRealtime() < resumeEnd && mWorkerEnabled) {
                    long now = SystemClock.elapsedRealtime();
                    // Some OEM hardware DSPs (e.g. Qualcomm Fluence AEC+NS) gate cancelled echo
                    // in a quiet room to exact digital zero (0x0000) while AEC is active. If blocks
                    // are arriving but all samples are 0x0000 in the first half of the window,
                    // briefly disable AEC/NS for the second half while speech is playing to verify
                    // the ADC/DMA is capturing live acoustic signal rather than stuck zero buffers.
                    if (!nonZeroVerified && !probedAecOffForLiveness && now >= probeSwitchTime) {
                        probedAecOffForLiveness = true;
                        if (aec != null) {
                            try {
                                aec.setEnabled(false);
                            } catch (Exception ignored) {
                            }
                        }
                        if (ns != null) {
                            try {
                                ns.setEnabled(false);
                            } catch (Exception ignored) {
                            }
                        }
                    }
                    int read = isNative
                            ? nativeReadStream(nativeRec, ioBuf5, 0, burst5, 100_000_000L)
                            : javaRec.read(ioBuf5, 0, burst5, AudioRecord.READ_BLOCKING);
                    if (read > 0) {
                        long elapsed = SystemClock.elapsedRealtime() - resumeStartMs;
                        if (firstReadMs < 0) {
                            firstReadMs = elapsed;
                            aecEnabledAfterWake =
                                    !mAecSupported || (aec != null && aec.getEnabled());
                        }
                        double blockRms = computeBlockRms(ioBuf5, read);
                        if (!nonZeroVerified && hasNonZeroSample(ioBuf5, read)) {
                            nonZeroVerified = true;
                        }
                        if (!probedAecOffForLiveness) {
                            postRmsSum += blockRms;
                            postCount++;
                        }
                    }
                }
                if (probedAecOffForLiveness && aec != null) {
                    try {
                        aec.setEnabled(true);
                    } catch (Exception ignored) {
                    }
                }

                if (isNative) {
                    row.standbyPostTsPos = nativeGetTimestampPosition(nativeRec);
                } else {
                    AudioTimestamp ts = new AudioTimestamp();
                    if (javaRec.getTimestamp(ts, AudioTimestamp.TIMEBASE_MONOTONIC)
                            == AudioRecord.SUCCESS) {
                        row.standbyPostTsPos = ts.framePosition;
                    }
                }
                player.setPlaySpeech(false);

                row.standbyExecuted = true;
                row.standbyFirstFrameMs = firstReadMs;
                double avgPostRms = (postCount > 0) ? (postRmsSum / postCount) : MIN_RMS_VAL;
                row.standbyPostRmsDbfs = 20.0 * Math.log10(Math.max(avgPostRms, MIN_RMS_VAL));
                row.standbyAecStillEnabled = aecEnabledAfterWake
                        && (!mAecSupported || (aec != null && aec.getEnabled()));

                boolean tsValid = isNative
                        ? ((row.standbyPreTsPos <= 0)
                                || (row.standbyPostTsPos >= row.standbyPreTsPos))
                        : (row.standbyPostTsPos > 0);
                row.standbyPassed = (startInRes == 0 && startOutRes == 0)
                        && (postCount > 0 && firstReadMs >= 0 && firstReadMs <= 600)
                        && nonZeroVerified
                        && tsValid
                        && row.standbyAecStillEnabled;

                updatePhaseStatus(
                        row,
                        5,
                        row.standbyPassed
                                ? String.format(
                                        Locale.US,
                                        "PASS (%dms, ts %s, AEC)",
                                        firstReadMs,
                                        isNative ? "monotonic" : "active")
                                : String.format(
                                        Locale.US,
                                        "FAIL (%dms, live=%b, ts %d->%d)",
                                        firstReadMs,
                                        nonZeroVerified,
                                        row.standbyPreTsPos,
                                        row.standbyPostTsPos));
                appendTelemetry(String.format(
                        Locale.US,
                        "    Post-Standby: %d ms | %.1f dBFS | live=%b | ts=%d->%d | AEC=%s -> %s\n",
                        firstReadMs,
                        row.standbyPostRmsDbfs,
                        nonZeroVerified,
                        row.standbyPreTsPos,
                        row.standbyPostTsPos,
                        row.standbyAecStillEnabled ? "ON" : "LOST",
                        row.standbyPassed ? "PASS" : "FAIL"));

                if (row.standbyPassed) {
                    recordPass();
                } else {
                    recordFail();
                    Finding f = new Finding();
                    f.id = "VOIP-STANDBY-RESUME-FAILURE";
                    f.severity = "FAIL";
                    f.title = "VoIP stream failed to resume cleanly after >3.5s multi-turn standby pause";
                    f.whatHappened = "After stopping the VOICE_COMMUNICATION stream for 3.5 seconds (allowing the HAL to enter idle standby) and restarting it, capture either stalled, returned dead zero samples, returned invalid timestamps, or lost its AcousticEchoCanceler state.";
                    f.androidContract = "AAudio.h & StreamDescriptor.aidl: Full-duplex VoIP streams paused for >3 seconds between conversational turns must resume delivering frames immediately on start() with valid timestamps and preserved audio effects.";
                    f.whyItMatters = "Conversational AI voice agents pause audio streams between user turns; standby resume bugs cause frozen microphone capture or broken AEC on subsequent turns.";
                    f.howToFix = "1. Ensure the Audio HAL starts the capture/playback PCM DMA driver when transitioning from IDLE to ACTIVE ('burst') after standby exit.\n  2. Preserve cumulative frame counters across MMAP FIFO recreation so getTimestamp().position does not jump backward after standby.";
                    addOrUpdateFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: After 3.5s standby, firstFrame=%d ms, blocksRead=%d, livePcm=%b, ts=%d -> %d, aecEnabled=%b.",
                                    row.apiLabel,
                                    firstReadMs,
                                    postCount,
                                    nonZeroVerified,
                                    row.standbyPreTsPos,
                                    row.standbyPostTsPos,
                                    row.standbyAecStillEnabled));
                }
            }
        } finally {
            if (aec != null) {
                try {
                    aec.release();
                } catch (Exception ignored) {
                }
            }
            if (ns != null) {
                try {
                    ns.release();
                } catch (Exception ignored) {
                }
            }
            if (nativeRec != 0) {
                nativeStopStream(nativeRec);
                nativeCloseStream(nativeRec);
            }
            if (javaRec != null) {
                try {
                    javaRec.stop();
                } catch (Exception ignored) {
                }
                javaRec.release();
            }
            if (player != null) {
                player.stop();
            }
        }
    }

    private void evaluateCrossApiComparison(ApiRunMetrics first, ApiRunMetrics second) {
        ApiRunMetrics nat = first.isNative ? first : (second.isNative ? second : null);
        ApiRunMetrics jav = !first.isNative ? first : (!second.isNative ? second : null);
        if (nat == null || jav == null || !nat.metricsPopulated || !jav.metricsPopulated) {
            return;
        }
        appendTelemetry("------------------------------------------------");
        appendTelemetry("CROSS-API COMPARISON (NATIVE AAUDIO vs. JAVA)");
        appendTelemetry("------------------------------------------------");
        if (nat.voipOffAcousticRtlMs > 0 && jav.voipOffAcousticRtlMs > 0) {
            double deltaAcoustic = jav.voipOffAcousticRtlMs - nat.voipOffAcousticRtlMs;
            appendTelemetry(String.format(
                    Locale.US,
                    "  VoIP Acoustic : Nat=%.1fms | Java=%.1fms (%+.1fms)",
                    nat.voipOffAcousticRtlMs,
                    jav.voipOffAcousticRtlMs,
                    deltaAcoustic));
        }
        if (nat.voipOnTimestampRtlMs > 0 && jav.voipOnTimestampRtlMs > 0) {
            double deltaTs = jav.voipOnTimestampRtlMs - nat.voipOnTimestampRtlMs;
            appendTelemetry(String.format(
                    Locale.US,
                    "  VoIP HW TS    : Nat=%.1fms | Java=%.1fms (%+.1fms)",
                    nat.voipOnTimestampRtlMs,
                    jav.voipOnTimestampRtlMs,
                    deltaTs));
        }
        appendTelemetry(String.format(
                Locale.US,
                "  VoIP Self-ERLE: Nat=%.1f dB | Java=%.1f dB\n",
                nat.selfErleDb,
                jav.selfErleDb));
    }

    private static boolean hasNonZeroSample(short[] buf, int count) {
        if (buf == null || count <= 0) return false;
        for (int i = 0; i < count; i++) {
            if (buf[i] != 0) return true;
        }
        return false;
    }

    private static double computeBlockRms(short[] buf, int count) {
        return computeBlockRmsScaled(buf, count, 1.0);
    }

    private static double computeBlockRmsScaled(short[] buf, int count, double gainScale) {
        if (buf == null || count <= 0) return MIN_RMS_VAL;
        double sumSq = 0.0;
        for (int i = 0; i < count; i++) {
            double v = (buf[i] / MAX_VAL) * gainScale;
            sumSq += v * v;
        }
        return Math.max(Math.sqrt(sumSq / count), MIN_RMS_VAL);
    }

    private static double computeEnvelopeCorrelation(double[] playerRms, double[] recRms) {
        return computeEnvelopeCorrelation(playerRms, recRms, 1.0, MIN_RMS_DB);
    }

    private static double computeEnvelopeCorrelation(
            double[] playerRms, double[] recRms, double recGainScale) {
        return computeEnvelopeCorrelation(playerRms, recRms, recGainScale, MIN_RMS_DB);
    }

    private static double computeEnvelopeCorrelation(
            double[] playerRms, double[] recRms, double recGainScale, double recFloorDb) {
        if (playerRms == null || recRms == null) return 0.0;
        int len = Math.min(playerRms.length, recRms.length);
        int last = len - 1;
        int first = Math.max(0, last - 15);
        int n = last - first + 1;
        if (n <= 2) return 0.0;

        double[] a = new double[n];
        double[] b = new double[n];
        double sumA = 0.0;
        double sumB = 0.0;
        for (int i = 0; i < n; i++) {
            a[i] = Math.max(20.0 * Math.log10(Math.max(playerRms[first + i], MIN_RMS_VAL)), MIN_RMS_DB);
            b[i] = Math.max(
                    20.0 * Math.log10(Math.max(recRms[first + i] * recGainScale, MIN_RMS_VAL)),
                    recFloorDb);
            sumA += a[i];
            sumB += b[i];
        }
        double meanA = sumA / n;
        double meanB = sumB / n;
        double varA = 0.0;
        double varB = 0.0;
        for (int i = 0; i < n; i++) {
            double da = a[i] - meanA;
            double db = b[i] - meanB;
            varA += da * da;
            varB += db * db;
        }
        varA /= n;
        varB /= n;
        double denom = Math.sqrt(varA * varB);
        if (denom <= 1e-7) return 0.0;

        double maxCorr = 0.0;
        for (int lag = 0; lag < n; lag++) {
            double cross = 0.0;
            for (int j = 0; j < n - lag; j++) {
                cross += (a[j] - meanA) * (b[lag + j] - meanB);
            }
            double corr = Math.abs(cross / (n * denom));
            if (corr > maxCorr) {
                maxCorr = corr;
            }
        }
        return maxCorr;
    }

    private static double computeAcousticLatencyMs(
            short[] ref, int refRate, short[] rec, int recRate, int maxLagMs) {
        if (ref == null || rec == null || refRate <= 0 || recRate <= 0) return -1.0;
        // Skip the first 500 ms so HAL routing and effect bypass are in steady state.
        int startRecSample = Math.min(recRate / 2, rec.length / 4);
        int startRefSample = (int) ((long) startRecSample * refRate / recRate);
        int availRec = rec.length - startRecSample;
        int availRef = ref.length - startRefSample;
        if (availRec <= 0 || availRef <= 0) return -1.0;

        int maxLagSamples = Math.min((recRate * maxLagMs) / 1000, availRec - 1);
        int windowRecSamples = Math.min(recRate * 2, availRec - maxLagSamples);
        int maxRefOffset = (int) ((long) windowRecSamples * refRate / recRate);
        if (maxRefOffset >= availRef) {
            windowRecSamples = (int) ((long) (availRef - 1) * recRate / refRate);
        }
        if (windowRecSamples <= 0) return -1.0;

        int coarseStride = 16;
        double maxCorr = -1.0;
        int bestLag = 0;

        for (int lag = 0; lag < maxLagSamples; lag += coarseStride) {
            double dot = 0.0;
            double normRef = 0.0;
            double normRec = 0.0;
            for (int i = 0; i < windowRecSamples; i += coarseStride) {
                int refIdx = startRefSample + (int) ((long) i * refRate / recRate);
                double r = ref[refIdx];
                double s = rec[startRecSample + lag + i];
                dot += r * s;
                normRef += r * r;
                normRec += s * s;
            }
            double norm = Math.sqrt(normRef * normRec);
            if (norm > 1e-4) {
                double corr = Math.abs(dot / norm);
                if (corr > maxCorr) {
                    maxCorr = corr;
                    bestLag = lag;
                }
            }
        }

        int fineStart = Math.max(0, bestLag - 32);
        int fineEnd = Math.min(maxLagSamples, bestLag + 32);
        for (int lag = fineStart; lag <= fineEnd; lag++) {
            double dot = 0.0;
            double normRef = 0.0;
            double normRec = 0.0;
            for (int i = 0; i < windowRecSamples; i += 4) {
                int refIdx = startRefSample + (int) ((long) i * refRate / recRate);
                double r = ref[refIdx];
                double s = rec[startRecSample + lag + i];
                dot += r * s;
                normRef += r * r;
                normRec += s * s;
            }
            double norm = Math.sqrt(normRef * normRec);
            if (norm > 1e-4) {
                double corr = Math.abs(dot / norm);
                if (corr > maxCorr) {
                    maxCorr = corr;
                    bestLag = lag;
                }
            }
        }
        return (bestLag * 1000.0) / recRate;
    }

    private double computeJavaTimestampLatencyMillis(
            VoipPlayer player, AudioRecord record, long framesRead) {
        if (player == null || player.getAudioTrack() == null || record == null) return -1.0;
        AudioTimestamp outTs = new AudioTimestamp();
        AudioTimestamp inTs = new AudioTimestamp();
        boolean gotOut = player.getAudioTrack().getTimestamp(outTs);
        int inStatus = record.getTimestamp(inTs, AudioTimestamp.TIMEBASE_MONOTONIC);
        if (!gotOut
                || inStatus != AudioRecord.SUCCESS
                || outTs.framePosition <= 0
                || inTs.framePosition <= 0) {
            return -1.0;
        }
        long framesWritten = player.getFramesWritten();
        long outPresNs = outTs.nanoTime
                + ((framesWritten - outTs.framePosition) * 1_000_000_000L) / SAMPLE_RATE;
        long inCapNs = inTs.nanoTime
                + ((framesRead - inTs.framePosition) * 1_000_000_000L) / SAMPLE_RATE;
        if (outPresNs < inCapNs) return -1.0;
        return (outPresNs - inCapNs) / 1_000_000.0;
    }

    private class VoipPlayer {
        private final boolean mIsNative;
        private final short[] mAudioData;
        private final int mAudioDataLen;
        private long mNativeHandle = 0;
        private AudioTrack mAudioTrack;
        private volatile boolean mPlaying = false;
        private volatile boolean mPlaySpeech = false;
        private volatile boolean mPausedJava = false;
        private volatile boolean mResetCursor = false;
        private volatile long mFramesWritten = 0;
        private volatile RmsEnvelopeTracker mRmsTracker;
        private Thread mThread;

        VoipPlayer(boolean isNative, short[] audioData) {
            mIsNative = isNative;
            mAudioData = audioData;
            mAudioDataLen = (audioData != null) ? audioData.length : 0;
        }

        void setRmsTracker(RmsEnvelopeTracker tracker) {
            mRmsTracker = tracker;
        }

        void setPlaySpeech(boolean play) {
            if (mIsNative && mNativeHandle != 0) {
                nativeSetPlaySpeech(mNativeHandle, play);
            }
            mPlaySpeech = play;
            mResetCursor = true;
        }

        void playSpeechFromStart() {
            setPlaySpeech(true);
        }

        long getNativeHandle() {
            return mNativeHandle;
        }

        AudioTrack getAudioTrack() {
            return mAudioTrack;
        }

        long getFramesWritten() {
            return mFramesWritten;
        }

        void pauseJavaTrack() {
            mPausedJava = true;
            if (mAudioTrack != null) {
                try {
                    mAudioTrack.pause();
                } catch (Exception ignored) {
                }
            }
        }

        void resumeJavaTrack() {
            if (mAudioTrack != null) {
                try {
                    mAudioTrack.play();
                } catch (Exception ignored) {
                }
            }
            mPausedJava = false;
        }

        boolean start() {
            if (mPlaying) return true;
            mFramesWritten = 0;
            if (mIsNative) {
                mNativeHandle = nativeOpenOutputStream(SAMPLE_RATE, 1);
                if (mNativeHandle == 0) return false;
                if (mAudioData != null) {
                    nativeSetPlaybackData(mNativeHandle, mAudioData);
                }
                nativeSetPlaySpeech(mNativeHandle, mPlaySpeech);
                if (nativeStartStream(mNativeHandle) != 0) {
                    nativeCloseStream(mNativeHandle);
                    mNativeHandle = 0;
                    return false;
                }
            } else {
                int minBuf = AudioTrack.getMinBufferSize(
                        SAMPLE_RATE,
                        AudioFormat.CHANNEL_OUT_MONO,
                        AudioFormat.ENCODING_PCM_16BIT);
                int bufferSize = Math.max(minBuf * 2, 7680);
                mAudioTrack = new AudioTrack.Builder()
                        .setAudioAttributes(new AudioAttributes.Builder()
                                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                                .build())
                        .setAudioFormat(new AudioFormat.Builder()
                                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                .setSampleRate(SAMPLE_RATE)
                                .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                                .build())
                        .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
                        .setBufferSizeInBytes(bufferSize)
                        .setTransferMode(AudioTrack.MODE_STREAM)
                        .build();
                mAudioTrack.play();
            }

            mPlaying = true;
            mThread = new Thread(this::runLoop, "VoipPlayerLoop");
            mThread.setPriority(Thread.MAX_PRIORITY);
            mThread.start();
            return true;
        }

        private void runLoop() {
            int burst = PLAYER_RMS_WINDOW_SAMPLES;
            short[] tempBuf = new short[burst];
            short[] silenceBuf = new short[burst];

            if (mIsNative && mNativeHandle != 0) {
                int lastOffset = nativeGetPlaybackOffset(mNativeHandle);
                while (mPlaying) {
                    if (mResetCursor) {
                        lastOffset = 0;
                        mResetCursor = false;
                    }
                    int currOffset = nativeGetPlaybackOffset(mNativeHandle);
                    RmsEnvelopeTracker tracker = mRmsTracker;
                    if (tracker != null && mPlaySpeech && mAudioData != null && mAudioDataLen > 0) {
                        int advanced = (currOffset - lastOffset + mAudioDataLen) % mAudioDataLen;
                        if (advanced > mAudioDataLen / 2) {
                            lastOffset = currOffset;
                            advanced = 0;
                        }
                        while (advanced > 0) {
                            int count = Math.min(advanced, burst);
                            for (int i = 0; i < count; i++) {
                                tempBuf[i] = mAudioData[(lastOffset + i) % mAudioDataLen];
                            }
                            tracker.updateDirect(tempBuf, count);
                            lastOffset = (lastOffset + count) % mAudioDataLen;
                            advanced -= count;
                        }
                    } else {
                        lastOffset = currOffset;
                    }
                    SystemClock.sleep(10);
                }
            } else if (mAudioTrack != null) {
                int offset = 0;
                int silenceChunk = 480; // 10 ms at 48 kHz
                while (mPlaying) {
                    if (mPausedJava) {
                        SystemClock.sleep(20);
                        continue;
                    }
                    if (mResetCursor) {
                        offset = 0;
                        mResetCursor = false;
                    }
                    if (mPlaySpeech && mAudioData != null && mAudioDataLen > 0) {
                        int toWrite = Math.min(burst, mAudioDataLen - offset);
                        System.arraycopy(mAudioData, offset, tempBuf, 0, toWrite);
                        if (toWrite < burst) {
                            System.arraycopy(mAudioData, 0, tempBuf, toWrite, burst - toWrite);
                            offset = burst - toWrite;
                        } else {
                            offset = (offset + toWrite) % mAudioDataLen;
                        }
                        RmsEnvelopeTracker tracker = mRmsTracker;
                        if (tracker != null) {
                            tracker.updateDirect(tempBuf, burst);
                        }
                        int written = mAudioTrack.write(
                                tempBuf, 0, burst, AudioTrack.WRITE_BLOCKING);
                        if (written < 0) break;
                        mFramesWritten += written;
                    } else {
                        long head = mAudioTrack.getPlaybackHeadPosition() & 0xFFFFFFFFL;
                        if (mFramesWritten - head > 960) {
                            SystemClock.sleep(5);
                            continue;
                        }
                        int written = mAudioTrack.write(
                                silenceBuf, 0, silenceChunk, AudioTrack.WRITE_BLOCKING);
                        if (written < 0) break;
                        mFramesWritten += written;
                    }
                }
            }
        }

        void stop() {
            mPlaying = false;
            mPausedJava = false;
            if (mThread != null) {
                try {
                    mThread.join(500);
                } catch (InterruptedException ignored) {
                }
                mThread = null;
            }
            if (mIsNative && mNativeHandle != 0) {
                nativeStopStream(mNativeHandle);
                nativeCloseStream(mNativeHandle);
                mNativeHandle = 0;
            }
            if (mAudioTrack != null) {
                try {
                    mAudioTrack.stop();
                } catch (Exception ignored) {
                }
                mAudioTrack.release();
                mAudioTrack = null;
            }
        }
    }

    private short[] ensureSpeechPcmDecoded() {
        synchronized (mSpeechDecodeLock) {
            if (mSpeechPcm != null) return mSpeechPcm;
            mSpeechPcm = decodeResourceToPcm(getApplicationContext(), R.raw.voice_speech_48k);
            if (mSpeechPcm == null || mSpeechPcm.length < SAMPLE_RATE) {
                mSpeechPcm = generateSyntheticSpeechFallback(SAMPLE_RATE, 6);
                mSpeechSampleRate = SAMPLE_RATE;
            }
            return mSpeechPcm;
        }
    }

    private short[] decodeResourceToPcm(Context context, int resId) {
        Uri uri = Uri.parse("android.resource://" + context.getPackageName() + "/" + resId);
        MediaExtractor extractor = new MediaExtractor();
        MediaCodec codec = null;
        try {
            extractor.setDataSource(context, uri, null);
            int trackIndex = -1;
            MediaFormat format = null;
            for (int i = 0; i < extractor.getTrackCount(); i++) {
                MediaFormat f = extractor.getTrackFormat(i);
                String mime = f.getString(MediaFormat.KEY_MIME);
                if (mime != null && mime.startsWith("audio/")) {
                    trackIndex = i;
                    format = f;
                    break;
                }
            }
            if (trackIndex < 0 || format == null) return null;
            extractor.selectTrack(trackIndex);

            MediaCodecList mclist = new MediaCodecList(MediaCodecList.ALL_CODECS);
            String decoderName = mclist.findDecoderForFormat(format);
            if (decoderName == null) return null;
            codec = MediaCodec.createByCodecName(decoderName);
            codec.configure(format, null, null, 0);
            codec.start();

            ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            boolean sawInputEOS = false;
            boolean sawOutputEOS = false;

            while (!sawOutputEOS) {
                if (!sawInputEOS) {
                    int inIdx = codec.dequeueInputBuffer(5000);
                    if (inIdx >= 0) {
                        ByteBuffer inBuf = codec.getInputBuffer(inIdx);
                        int sampleSize = extractor.readSampleData(inBuf, 0);
                        if (sampleSize < 0) {
                            sawInputEOS = true;
                            codec.queueInputBuffer(
                                    inIdx, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                        } else {
                            long ptsUs = extractor.getSampleTime();
                            codec.queueInputBuffer(inIdx, 0, sampleSize, ptsUs, 0);
                            extractor.advance();
                        }
                    }
                }
                int outIdx = codec.dequeueOutputBuffer(info, 5000);
                if (outIdx >= 0) {
                    ByteBuffer outBuf = codec.getOutputBuffer(outIdx);
                    byte[] chunk = new byte[info.size];
                    outBuf.get(chunk);
                    outBuf.clear();
                    byteStream.write(chunk, 0, chunk.length);
                    codec.releaseOutputBuffer(outIdx, false);
                    if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        sawOutputEOS = true;
                    }
                }
            }
            if (format.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
                mSpeechSampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
            }
            byte[] pcmBytes = byteStream.toByteArray();
            short[] pcmShorts = new short[pcmBytes.length / 2];
            ByteBuffer.wrap(pcmBytes).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(pcmShorts);
            return pcmShorts;
        } catch (Exception e) {
            Log.e(TAG, "Failed to decode voice_speech_48k resource", e);
            return null;
        } finally {
            if (codec != null) {
                try {
                    codec.stop();
                    codec.release();
                } catch (Exception ignored) {
                }
            }
            extractor.release();
        }
    }

    private static short[] generateSyntheticSpeechFallback(int sampleRate, int seconds) {
        int total = sampleRate * seconds;
        short[] out = new short[total];
        long seed = 0x5A17C9E3L;
        for (int i = 0; i < total; i++) {
            double t = (double) i / sampleRate;
            // Syllable-rate envelope modulation (3.2 Hz + 1.3 Hz pauses)
            double syllEnv = Math.max(0.0, Math.sin(2.0 * Math.PI * 3.2 * t)
                    * (0.6 + 0.4 * Math.cos(2.0 * Math.PI * 1.3 * t)));
            double f0 = 140.0 + 35.0 * Math.sin(2.0 * Math.PI * 2.1 * t);
            double voiced = 0.50 * Math.sin(2.0 * Math.PI * f0 * t)
                    + 0.30 * Math.sin(2.0 * Math.PI * f0 * 2.0 * t)
                    + 0.15 * Math.sin(2.0 * Math.PI * f0 * 3.0 * t);
            seed = (seed * 6364136223846793005L) + 1442695040888963407L;
            double noise = ((seed >> 33) / (double) (1L << 30)) * 0.08;
            double sample = syllEnv * (voiced + noise) * 18000.0;
            out[i] = (short) Math.max(-32000.0, Math.min(32000.0, sample));
        }
        return out;
    }

    private void playCapturedBuffer(short[] buffer, int length) {
        if (buffer == null || length <= 0 || mIsRunning) return;
        final short[] copy = Arrays.copyOf(buffer, length);
        new Thread(() -> {
            int minBuf = AudioTrack.getMinBufferSize(
                    SAMPLE_RATE,
                    AudioFormat.CHANNEL_OUT_MONO,
                    AudioFormat.ENCODING_PCM_16BIT);
            AudioTrack track = new AudioTrack.Builder()
                    .setAudioAttributes(new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                            .build())
                    .setAudioFormat(new AudioFormat.Builder()
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                            .setSampleRate(SAMPLE_RATE)
                            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                            .build())
                    .setBufferSizeInBytes(Math.max(minBuf * 4, 16384))
                    .setTransferMode(AudioTrack.MODE_STREAM)
                    .build();
            try {
                track.play();
                track.write(copy, 0, copy.length);
                track.stop();
            } catch (Exception e) {
                Log.e(TAG, "Diagnostic playback failed", e);
            } finally {
                track.release();
            }
        }).start();
    }

    private boolean sleepInterruptible(int durationMs) {
        int remaining = durationMs;
        while (remaining > 0 && mWorkerEnabled) {
            int step = Math.min(25, remaining);
            SystemClock.sleep(step);
            remaining -= step;
        }
        return mWorkerEnabled;
    }

    private void setProgress(String progress) {
        synchronized (mReportLock) {
            mCurrentProgress = progress;
        }
    }

    private void appendTelemetry(String line) {
        Log.i(TAG, "[VoipAecDiag] " + line);
        synchronized (mReportLock) {
            mTelemetryLog.append(line).append("\n");
        }
    }

    private void updatePhaseStatus(ApiRunMetrics row, int phase, String status) {
        synchronized (mReportLock) {
            switch (phase) {
                case 1:
                    row.phase1Status = status;
                    break;
                case 2:
                    row.phase2Status = status;
                    break;
                case 3:
                    row.phase3Status = status;
                    break;
                case 4:
                    row.phase4Status = status;
                    break;
                case 5:
                    row.phase5Status = status;
                    break;
            }
        }
    }

    private void addOrUpdateFinding(Finding finding, String pathLabel, String evidence) {
        synchronized (mReportLock) {
            for (Finding existing : mFindings) {
                if (existing.id.equals(finding.id)) {
                    if ("FAIL".equals(finding.severity)) {
                        existing.severity = "FAIL";
                    }
                    if (!existing.affectedPaths.contains(pathLabel)) {
                        existing.affectedPaths.add(pathLabel);
                    }
                    existing.measuredEvidence.add(evidence);
                    return;
                }
            }
            finding.affectedPaths.add(pathLabel);
            finding.measuredEvidence.add(evidence);
            mFindings.add(finding);
        }
    }

    private void recordPass() {
        synchronized (mReportLock) {
            mPassCount++;
        }
    }

    private void recordWarn() {
        synchronized (mReportLock) {
            mWarnCount++;
        }
    }

    private void recordFail() {
        synchronized (mReportLock) {
            mFailCount++;
        }
    }

    private static String formatMs(double ms) {
        return (ms > 0.0) ? String.format(Locale.US, "%.1f ms", ms) : "N/A";
    }

    private String buildFullReport() {
        synchronized (mReportLock) {
            StringBuilder out = new StringBuilder();
            int totalChecks = mPassCount + mWarnCount + mFailCount;
            String verdict = "RUNNING...";
            if (mSuiteCompleted) {
                if (mSuiteStoppedByUser) {
                    verdict = "STOPPED (cancelled by user)";
                } else if (mFailCount > 0) {
                    verdict = String.format(
                            Locale.US,
                            "FAIL (%d issue%s detected)",
                            mFindings.size(),
                            mFindings.size() == 1 ? "" : "s");
                } else if (mWarnCount > 0) {
                    verdict = String.format(
                            Locale.US,
                            "WARN (%d issue%s detected)",
                            mFindings.size(),
                            mFindings.size() == 1 ? "" : "s");
                } else {
                    verdict = "PASS";
                }
            }

            out.append("================================================\n");
            out.append("1. SUMMARY & SCORECARD\n");
            out.append("================================================\n");
            out.append(String.format(Locale.US, "Overall Verdict : %s\n", verdict));
            if (!mSuiteCompleted) {
                out.append(String.format(Locale.US, "Current Status  : %s\n", mCurrentProgress));
            }
            out.append(String.format(
                    Locale.US,
                    "Checks Executed : %d (%d PASS, %d WARN, %d FAIL)\n",
                    totalChecks, mPassCount, mWarnCount, mFailCount));
            out.append(String.format(
                    Locale.US,
                    "Capabilities    : AEC=%s | NS=%s | MMap=%s (Excl=%s)\n\n",
                    mAecSupported ? "YES" : "NO",
                    mNsSupported ? "YES" : "NO",
                    mMmapSupported ? "YES" : "NO",
                    mMmapExclusiveSupported ? "YES" : "NO"));

            out.append("SCORECARD BY STREAM:\n");
            for (ApiRunMetrics row : mScorecard) {
                out.append(String.format(
                        Locale.US, "* %s (%s)\n", row.apiLabel, row.configSummary));
                out.append(String.format(
                        Locale.US, "  [1] Silence : %s\n", row.phase1Status));
                out.append(String.format(
                        Locale.US, "  [2] Raw Echo: %s\n", row.phase2Status));
                out.append(String.format(
                        Locale.US, "  [3] AEC OFF : %s\n", row.phase3Status));
                out.append(String.format(
                        Locale.US, "  [4] AEC ON  : %s\n", row.phase4Status));
                out.append(String.format(
                        Locale.US, "  [5] Standby : %s\n", row.phase5Status));
            }
            out.append("\n");

            out.append("================================================\n");
            out.append("2. VOIP LATENCY ATTRIBUTION & AEC METRICS\n");
            out.append("================================================\n");
            boolean anyMetrics = false;
            ApiRunMetrics natMetrics = null;
            ApiRunMetrics javaMetrics = null;
            for (ApiRunMetrics row : mScorecard) {
                if (!row.metricsPopulated) continue;
                anyMetrics = true;
                if (row.isNative) {
                    natMetrics = row;
                } else {
                    javaMetrics = row;
                }
                out.append(String.format(Locale.US, "* %s:\n", row.apiLabel));
                out.append(String.format(
                        Locale.US,
                        "  - Raw RTL (VOICE_REC) : %.1fms (TS %s)\n",
                        row.rawAcousticRtlMs,
                        formatMs(row.rawTimestampRtlMs)));
                out.append(String.format(
                        Locale.US,
                        "  - VoIP RTL (AEC OFF)  : %.1fms (TS %s)\n",
                        row.voipOffAcousticRtlMs,
                        formatMs(row.voipOffTimestampRtlMs)));
                out.append(String.format(
                        Locale.US,
                        "  - VoIP RTL (AEC ON)   : TS %s (<=100ms)\n",
                        formatMs(row.voipOnTimestampRtlMs)));
                if (row.rawAcousticRtlMs > 0 && row.voipOffAcousticRtlMs > 0) {
                    double routeOverhead = row.voipOffAcousticRtlMs - row.rawAcousticRtlMs;
                    out.append(String.format(
                            Locale.US,
                            "  - VoIP Route Overhead : %+.1f ms vs. Raw\n",
                            routeOverhead));
                }
                out.append(String.format(
                        Locale.US,
                        "  - Levels (Noise/Raw)  : %.1f / %.1f dBFS\n",
                        row.noiseFloorDbfs,
                        row.rawCouplingDbfs));
                out.append(String.format(
                        Locale.US,
                        "  - Levels (Off / On)   : %.1f / %.1f dBFS\n",
                        row.aecOffDbfs,
                        row.aecOnDbfs));
                out.append(String.format(
                        Locale.US,
                        "  - Scaled (Off / On)   : %.1f / %.1f dBFS (Scale %+.1f dB)\n",
                        row.scaledAecOffDbfs,
                        row.scaledAecOnDbfs,
                        row.voipGainScaleDb));
                out.append(String.format(
                        Locale.US,
                        "  - ERLE (Norm / Raw)   : %.1f dB / %.1f dB\n",
                        row.selfErleDb,
                        row.rawErleDb));
                out.append(String.format(
                        Locale.US,
                        "  - Envelope Corr       : Off=%.2f | On=%.2f\n",
                        row.corrAecOff,
                        row.corrAecOn));
                out.append(String.format(
                        Locale.US,
                        "  - Preset Gain Offset  : %+.1f dB (scaled by %+.1f dB)\n\n",
                        row.signedGainDeltaDb,
                        row.voipGainScaleDb));
            }
            if (natMetrics != null && javaMetrics != null) {
                out.append("* JAVA vs. NATIVE AAUDIO VOIP COMPARISON:\n");
                if (natMetrics.voipOffAcousticRtlMs > 0 && javaMetrics.voipOffAcousticRtlMs > 0) {
                    out.append(String.format(
                            Locale.US,
                            "  - Acoustic RTL  : Java %.1f | Nat %.1fms (%+.1f)\n",
                            javaMetrics.voipOffAcousticRtlMs,
                            natMetrics.voipOffAcousticRtlMs,
                            javaMetrics.voipOffAcousticRtlMs - natMetrics.voipOffAcousticRtlMs));
                }
                if (natMetrics.voipOnTimestampRtlMs > 0 && javaMetrics.voipOnTimestampRtlMs > 0) {
                    out.append(String.format(
                            Locale.US,
                            "  - HW Timestamp  : Java %.1f | Nat %.1fms (%+.1f)\n",
                            javaMetrics.voipOnTimestampRtlMs,
                            natMetrics.voipOnTimestampRtlMs,
                            javaMetrics.voipOnTimestampRtlMs - natMetrics.voipOnTimestampRtlMs));
                }
                out.append("\n");
            }
            if (!anyMetrics) {
                out.append("Waiting for 4-Phase AEC + RTL measurements...\n\n");
            }

            out.append("================================================\n");
            out.append("3. DETECTED ISSUES & HOW TO FIX\n");
            out.append("================================================\n");
            if (mFindings.isEmpty()) {
                if (mSuiteCompleted) {
                    out.append("No VoIP, AEC, or multi-turn standby issues detected.\n\n");
                } else {
                    out.append("No issues detected so far (running)...\n\n");
                }
            } else {
                for (int i = 0; i < mFindings.size(); i++) {
                    Finding f = mFindings.get(i);
                    if (i > 0) {
                        out.append("------------------------------------------------\n");
                    }
                    out.append("[ISSUE #").append(i + 1).append("] ").append(f.title).append("\n");
                    out.append("- Severity : ").append(f.severity).append("\n");
                    out.append("- Streams  : ")
                            .append(String.join("; ", f.affectedPaths))
                            .append("\n\n");
                    out.append("- What Happened:\n  ").append(f.whatHappened).append("\n\n");
                    out.append("- Measured:\n");
                    for (String ev : f.measuredEvidence) {
                        out.append("  * ").append(ev).append("\n");
                    }
                    out.append("\n");
                    out.append("- Android Contract:\n  ").append(f.androidContract).append("\n\n");
                    out.append("- Why It Matters:\n  ").append(f.whyItMatters).append("\n\n");
                    out.append("- How to Fix:\n  ").append(f.howToFix).append("\n\n");
                }
            }

            out.append("================================================\n");
            out.append("4. PHASE TELEMETRY LOG\n");
            out.append("================================================\n");
            out.append(mTelemetryLog);
            return out.toString();
        }
    }

    private class ReportPoller extends Thread {
        volatile boolean mEnabled = true;

        @Override
        public void run() {
            while (mEnabled && mIsRunning) {
                updateReportUI();
                try {
                    sleep(200);
                } catch (InterruptedException e) {
                    break;
                }
            }
            if (!mEnabled) return;
            final String finalReport = mDeviceHeader + "\n" + buildFullReport();
            saveReportToFile(finalReport);
            updateReportUI();
            runOnUiThread(() -> {
                keepScreenOn(mIsRunning);
                setButtonsEnabled(mIsRunning);
            });
        }

        void finish() {
            mEnabled = false;
            interrupt();
            try {
                join(1000);
            } catch (InterruptedException ignored) {
            }
        }
    }

    private void saveReportToFile(String report) {
        File dir = getExternalFilesDir(null);
        if (dir == null) return;
        File outFile = new File(dir, REPORT_FILENAME);
        try (FileOutputStream fos = new FileOutputStream(outFile)) {
            fos.write(report.getBytes(StandardCharsets.UTF_8));
            Log.i(TAG, "Saved VoIP & AEC report to: " + outFile.getAbsolutePath());
        } catch (IOException e) {
            Log.e(TAG, "Failed to write VoIP & AEC report", e);
        }
    }

    private SpannableString styleReportForDisplay(String text) {
        SpannableString span = new SpannableString(text);
        int len = text.length();
        int lineStart = 0;
        while (lineStart < len) {
            int lineEnd = text.indexOf('\n', lineStart);
            if (lineEnd < 0) lineEnd = len;
            String line = text.substring(lineStart, lineEnd);
            if (line.startsWith("====")
                    || line.startsWith("1. SUMMARY")
                    || line.startsWith("2. VOIP LATENCY")
                    || line.startsWith("3. DETECTED")
                    || line.startsWith("4. PHASE")
                    || line.startsWith("SCORECARD BY STREAM:")
                    || line.startsWith("STREAM: ")
                    || line.startsWith("CROSS-API COMPARISON")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                span.setSpan(new ForegroundColorSpan(COLOR_HEADER), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if (line.startsWith("[ISSUE #")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                span.setSpan(new ForegroundColorSpan(COLOR_FAIL), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if (line.startsWith("* NATIVE") || line.startsWith("* JAVA")
                    || line.startsWith("  [Phase ")) {
                span.setSpan(new StyleSpan(Typeface.BOLD), lineStart, lineEnd,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            } else if ((line.startsWith("- ") || line.startsWith("  - "))
                    && line.indexOf(':') > 0 && line.indexOf(':') < 28) {
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
        final boolean running = mIsRunning;
        final String fullReport = mDeviceHeader + "\n" + buildFullReport();
        final SpannableString styled = styleReportForDisplay(fullReport);
        runOnUiThread(() -> {
            mStatusView.setText(styled);
            if (mScrollView != null && !running) {
                mScrollView.fullScroll(View.FOCUS_UP);
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

    private static native boolean nativeIsMMapSupported();
    private static native boolean nativeIsMMapExclusiveSupported();
    private native long nativeOpenOutputStream(int sampleRate, int channelCount);
    private native long nativeOpenInputStream(int sampleRate, int channelCount, int inputPreset);
    private native void nativeSetPlaybackData(long streamHandle, short[] audioData);
    private native void nativeSetPlaySpeech(long streamHandle, boolean playSpeech);
    private native int nativeGetPlaybackOffset(long streamHandle);
    private native int nativeStartStream(long streamHandle);
    private native int nativeStopStream(long streamHandle);
    private native void nativeCloseStream(long streamHandle);
    private native int nativeReadStream(
            long streamHandle, short[] buffer, int offset, int numSamples, long timeoutNanos);
    private native int nativeGetSessionId(long streamHandle);
    private native int nativeGetSampleRate(long streamHandle);
    private native int nativeGetBurstFrames(long streamHandle);
    private native int nativeGetBufferCapacityFrames(long streamHandle);
    private native boolean nativeIsExclusive(long streamHandle);
    private native boolean nativeIsMMap(long streamHandle);
    private native long nativeGetFramesWritten(long streamHandle);
    private native long nativeGetFramesRead(long streamHandle);
    private native long nativeGetTimestampPosition(long streamHandle);
    private native double nativeGetRoundTripTimestampLatencyMillis(
            long outStreamHandle, long inStreamHandle);
}
