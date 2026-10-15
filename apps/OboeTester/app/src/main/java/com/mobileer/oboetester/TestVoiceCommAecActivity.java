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
import static com.mobileer.oboetester.VoipAecAnalyzer.SAMPLE_RATE;
import static com.mobileer.oboetester.VoipAecTestEngine.formatMs;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
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
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.mobileer.oboetester.VoipAecAnalyzer.ApiRunMetrics;
import com.mobileer.oboetester.VoipAecAnalyzer.DecodedSpeech;
import com.mobileer.oboetester.VoipAecAnalyzer.Finding;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Activity to diagnose and tune the full-duplex Voice Communication (VoIP & Voice Agent) path
 * using a 5-phase self-referenced {@code VOICE_COMMUNICATION} architecture:
 * <ul>
 *   <li>Phase 1: Ambient Silence Floor ({@code VOICE_COMMUNICATION}, {@code AEC OFF}, Silence).</li>
 *   <li>Phase 2: Active AEC Silence Floor ({@code VOICE_COMMUNICATION}, {@code AEC ON}, Silence).</li>
 *   <li>Phase 3: Uncancelled VoIP Echo & RTL ({@code VOICE_COMMUNICATION}, {@code AEC OFF}, Speech).</li>
 *   <li>Phase 4: Cancelled VoIP Echo & ERLE ({@code VOICE_COMMUNICATION}, {@code AEC ON}, Speech).</li>
 *   <li>Phase 5: Multi-Turn Voice Agent Standby ({@code VOICE_COMMUNICATION}, {@code >3.5s} Pause + Resume).</li>
 * </ul>
 */
public class TestVoiceCommAecActivity extends AppCompatActivity {

    public static final String TAG = "OboeTester";
    public static final String KEY_AUTO_RUN = "auto_run";
    public static final String KEY_NATIVE = "native";
    public static final String KEY_JAVA = "java";
    public static final String KEY_FORCE_NS_OFF = "force_ns_off";
    public static final String KEY_AEC_PHASES = "aec_phases";
    public static final String KEY_STANDBY = "standby";
    public static final String REPORT_FILENAME = "voice_comm_aec_report.txt";

    private static final int MY_PERMISSIONS_REQUEST_RECORD_AUDIO = 938357;

    private static final int COLOR_HEADER = Color.parseColor("#0D47A1");
    private static final int COLOR_PASS = Color.parseColor("#2E7D32");
    private static final int COLOR_WARN = Color.parseColor("#E65100");
    private static final int COLOR_FAIL = Color.parseColor("#C62828");

    private static final Pattern PATTERN_PASS =
            Pattern.compile("(?<!\\b0 )\\bPASS\\b|\\[OK\\s*\\]");
    private static final Pattern PATTERN_WARN =
            Pattern.compile("(?<!\\b0 )\\bWARN(?:ING)?(?: \\([^)\\n]+\\))?");
    private static final Pattern PATTERN_FAIL =
            Pattern.compile("(?<!\\b0 )\\bFAIL(?: \\([^)\\n]+\\))?|\\[FAIL\\]");

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
    private final Object mAuditionLock = new Object();
    private short[] mSpeechPcm;
    private int mSpeechSampleRate = SAMPLE_RATE;

    private volatile boolean mWorkerEnabled = false;
    private volatile boolean mIsRunning = false;
    private Thread mWorkerThread;
    private ReportPoller mPoller;
    private Thread mAuditionThread;
    private AudioTrack mAuditionTrack;
    private String mReportFilePath;
    private boolean mAutoRun = false;

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
                + "\nTap START to run 5-Phase VoIP & AEC Diagnostic (no external rig needed):\n"
                + "  [1] Ambient Silence Floor (VOICE_COMMUNICATION, AEC OFF)\n"
                + "  [2] Active AEC Silence Floor (VOICE_COMMUNICATION, AEC ON)\n"
                + "  [3] Uncancelled VoIP Echo & RTL (VOICE_COMMUNICATION, AEC OFF)\n"
                + "  [4] Cancelled VoIP Echo & ERLE (VOICE_COMMUNICATION, AEC ON)\n"
                + "  [5] Multi-Turn Voice Agent Standby (>3.5s Pause + Resume)\n"));

        setButtonsEnabled(false);

        new Thread(this::ensureSpeechPcmDecoded).start();

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
        if (intent.hasExtra(KEY_NATIVE)) {
            mNativeApiCheckBox.setChecked(intent.getBooleanExtra(KEY_NATIVE, true));
        }
        if (intent.hasExtra(KEY_JAVA)) {
            mJavaApiCheckBox.setChecked(intent.getBooleanExtra(KEY_JAVA, true));
        }
        if (intent.hasExtra(KEY_FORCE_NS_OFF)) {
            mForceNsOffCheckBox.setChecked(intent.getBooleanExtra(KEY_FORCE_NS_OFF, false));
        }
        if (intent.hasExtra(KEY_AEC_PHASES)) {
            mPhaseAecCheckBox.setChecked(intent.getBooleanExtra(KEY_AEC_PHASES, true));
        }
        if (intent.hasExtra(KEY_STANDBY)) {
            mPhaseStandbyCheckBox.setChecked(intent.getBooleanExtra(KEY_STANDBY, true));
        }
        String customFile = intent.getStringExtra(IntentBasedTestSupport.KEY_FILE_NAME);
        mReportFilePath = (customFile != null && !customFile.isEmpty()) ? customFile : null;
        mAutoRun = intent.getBooleanExtra(KEY_AUTO_RUN, false);
        if (mAutoRun) {
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
        mAutoRun = false;
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
        final boolean testNative = mNativeApiCheckBox.isChecked();
        final boolean testJava = mJavaApiCheckBox.isChecked();
        final boolean forceNsOff = mForceNsOffCheckBox.isChecked();
        final boolean runAecPhases = mPhaseAecCheckBox.isChecked();
        final boolean runStandby = mPhaseStandbyCheckBox.isChecked();

        if ((!testNative && !testJava) || (!runAecPhases && !runStandby)) {
            Toast.makeText(
                    this,
                    "Select at least one API path and one test phase",
                    Toast.LENGTH_SHORT).show();
            return;
        }

        mDeviceHeader = buildDeviceHeader();
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            if (!mAutoRun) {
                ActivityCompat.requestPermissions(
                        this,
                        new String[]{Manifest.permission.RECORD_AUDIO},
                        MY_PERMISSIONS_REQUEST_RECORD_AUDIO);
                return;
            } else {
                Log.e(TAG, "RECORD_AUDIO permission not granted during auto_run.");
                synchronized (mReportLock) {
                    mTelemetryLog.setLength(0);
                    mScorecard.clear();
                    mFindings.clear();
                    mSuiteCompleted = true;
                    mSuiteStoppedByUser = false;
                    mPassCount = 0;
                    mWarnCount = 0;
                    mFailCount = 1;
                    mTelemetryLog.append("ERROR: RECORD_AUDIO permission not granted.\n");
                }
                final String finalReport = mDeviceHeader + "\n" + buildFullReport();
                saveReportToFile(finalReport);
                updateReportUI();
                return;
            }
        }

        stopTest();
        stopAuditionPlayback();
        keepScreenOn(true);

        mLastRecordingAecOff = null;
        mLastRecordingAecOffLen = 0;
        mLastRecordingAecOn = null;
        mLastRecordingAecOnLen = 0;

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
                row.phase1Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase2Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase3Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase4Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase5Status = runStandby ? "PENDING" : "SKIP";
                mScorecard.add(row);
            }
            if (testJava) {
                ApiRunMetrics row = new ApiRunMetrics(false, "JAVA API");
                row.phase1Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase2Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase3Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase4Status = runAecPhases ? "PENDING" : "SKIP";
                row.phase5Status = runStandby ? "PENDING" : "SKIP";
                mScorecard.add(row);
            }
        }

        mWorkerEnabled = true;
        mIsRunning = true;
        setButtonsEnabled(true);

        mWorkerThread = new Thread(
                () -> runDiagnosticSuite(forceNsOff, runAecPhases, runStandby),
                "VoiceCommAecWorker");
        mWorkerThread.start();

        mPoller = new ReportPoller();
        mPoller.start();
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
        }
    }

    private void stopTest() {
        stopAuditionPlayback();
        if (!mIsRunning && mWorkerThread == null && (mPoller == null || !mPoller.isAlive())) {
            mPoller = null;
            return;
        }
        keepScreenOn(false);
        stopPoller();
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

    @SuppressWarnings("deprecation")
    private void runDiagnosticSuite(
            boolean forceNsOff, boolean runAecPhases, boolean runStandby) {
        AudioManager am = getSystemService(AudioManager.class);
        int originalMode = (am != null) ? am.getMode() : AudioManager.MODE_NORMAL;
        int originalVoiceVol = -1;
        boolean originalSpeakerphone = false;
        boolean speakerphoneChanged = false;
        AudioDeviceInfo speakerDevice = null;

        final VoipAecTestEngine.Callbacks callbacks = new VoipAecTestEngine.Callbacks() {
            @Override
            public boolean isWorkerEnabled() {
                return mWorkerEnabled;
            }

            @Override
            public void onProgress(String progress) {
                setProgress(progress);
            }

            @Override
            public void onTelemetry(String line) {
                appendTelemetry(line);
            }

            @Override
            public void onPhaseStatus(ApiRunMetrics row, int phase, String status) {
                updatePhaseStatus(row, phase, status);
            }

            @Override
            public void onPass() {
                recordPass();
            }

            @Override
            public void onWarn() {
                recordWarn();
            }

            @Override
            public void onFail() {
                recordFail();
            }

            @Override
            public void onFinding(Finding finding, String pathLabel, String evidence) {
                addOrUpdateFinding(finding, pathLabel, evidence);
            }

            @Override
            public void onCaptureRecorded(
                    String apiLabel,
                    short[] aecOff,
                    int aecOffLen,
                    short[] aecOn,
                    int aecOnLen) {
                mLastRecordingAecOff = aecOff;
                mLastRecordingAecOffLen = aecOffLen;
                mLastRecordingAecOn = aecOn;
                mLastRecordingAecOnLen = aecOnLen;
            }
        };

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
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    for (AudioDeviceInfo dev : am.getAvailableCommunicationDevices()) {
                        if (dev.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
                            speakerDevice = dev;
                            break;
                        }
                    }
                }
                if (speakerDevice != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
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
                    originalSpeakerphone = am.isSpeakerphoneOn();
                    am.setSpeakerphoneOn(true);
                    speakerphoneChanged = true;
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
                    if (!VoipAecTestEngine.sleepInterruptible(1200, callbacks)) break;
                }
                VoipAecTestEngine.runSingleApiPath(
                        rowsCopy.get(i),
                        speechPcm,
                        mSpeechSampleRate,
                        am,
                        mAecSupported,
                        mNsSupported,
                        mMmapSupported,
                        forceNsOff,
                        runAecPhases,
                        runStandby,
                        callbacks);
            }

            if (mWorkerEnabled && rowsCopy.size() == 2) {
                VoipAecTestEngine.evaluateCrossApiComparison(
                        rowsCopy.get(0), rowsCopy.get(1), callbacks);
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
                    if (speakerDevice != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        am.clearCommunicationDevice();
                    }
                    if (speakerphoneChanged) {
                        am.setSpeakerphoneOn(originalSpeakerphone);
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

    private short[] ensureSpeechPcmDecoded() {
        synchronized (mSpeechDecodeLock) {
            if (mSpeechPcm != null) return mSpeechPcm;
            DecodedSpeech decoded = VoipAecAnalyzer.decodeSpeechResource(
                    getApplicationContext(), R.raw.voice_speech_48k);
            mSpeechPcm = decoded.pcm;
            mSpeechSampleRate = decoded.sampleRate;
            return mSpeechPcm;
        }
    }

    private void stopAuditionPlayback() {
        Thread threadToJoin;
        synchronized (mAuditionLock) {
            threadToJoin = mAuditionThread;
            mAuditionThread = null;
            if (mAuditionTrack != null) {
                try {
                    mAuditionTrack.pause();
                    mAuditionTrack.flush();
                    mAuditionTrack.stop();
                } catch (Exception ignored) {
                }
            }
        }
        if (threadToJoin != null) {
            threadToJoin.interrupt();
            try {
                threadToJoin.join(300);
            } catch (InterruptedException ignored) {
            }
        }
    }

    private void playCapturedBuffer(short[] buffer, int length) {
        if (buffer == null || length <= 0 || mIsRunning) return;
        stopAuditionPlayback();
        final short[] copy = Arrays.copyOf(buffer, length);
        Thread playThread = new Thread(() -> {
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
            synchronized (mAuditionLock) {
                if (Thread.currentThread().isInterrupted()) {
                    track.release();
                    return;
                }
                mAuditionTrack = track;
            }
            try {
                track.play();
                int offset = 0;
                int chunk = 2048;
                while (offset < copy.length && !Thread.currentThread().isInterrupted()) {
                    int toWrite = Math.min(chunk, copy.length - offset);
                    int written = track.write(copy, offset, toWrite);
                    if (written <= 0) break;
                    offset += written;
                }
                track.stop();
            } catch (Exception e) {
                Log.e(TAG, "Diagnostic playback failed", e);
            } finally {
                synchronized (mAuditionLock) {
                    if (mAuditionTrack == track) {
                        mAuditionTrack = null;
                    }
                    if (mAuditionThread == Thread.currentThread()) {
                        mAuditionThread = null;
                    }
                }
                track.release();
            }
        }, "AuditionPlayback");
        synchronized (mAuditionLock) {
            mAuditionThread = playThread;
        }
        playThread.start();
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
                } else if (mPassCount > 0) {
                    verdict = "PASS";
                } else {
                    verdict = "SKIPPED (0 checks executed)";
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

            out.append("SCORECARD BY STREAM (VOICE_COMMUNICATION):\n");
            for (ApiRunMetrics row : mScorecard) {
                out.append(String.format(
                        Locale.US, "* %s (%s)\n", row.apiLabel, row.configSummary));
                out.append(String.format(
                        Locale.US, "  [1] Silence (AEC OFF) : %s\n", row.phase1Status));
                out.append(String.format(
                        Locale.US, "  [2] Silence (AEC ON)  : %s\n", row.phase2Status));
                out.append(String.format(
                        Locale.US, "  [3] Echo    (AEC OFF) : %s\n", row.phase3Status));
                out.append(String.format(
                        Locale.US, "  [4] Echo    (AEC ON)  : %s\n", row.phase4Status));
                out.append(String.format(
                        Locale.US, "  [5] Standby (>3.5s)   : %s\n", row.phase5Status));
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
                        "  - VoIP RTL (AEC OFF)  : %.1fms (TS %s)\n",
                        row.voipOffAcousticRtlMs,
                        formatMs(row.voipOffTimestampRtlMs)));
                out.append(String.format(
                        Locale.US,
                        "  - VoIP RTL (AEC ON)   : TS %s (<=100ms)\n",
                        formatMs(row.voipOnTimestampRtlMs)));
                out.append(String.format(
                        Locale.US,
                        "  - Silence (Off / On)  : %.1f / %.1f dBFS\n",
                        row.noiseFloorDbfs,
                        row.aecOnSilenceDbfs));
                out.append(String.format(
                        Locale.US,
                        "  - Speech  (Off / On)  : %.1f / %.1f dBFS\n",
                        row.aecOffDbfs,
                        row.aecOnDbfs));
                out.append(String.format(
                        Locale.US,
                        "  - Stimulus SNR        : %.1f dB (>= 12.0 dB)\n",
                        row.stimulusSnrDb));
                out.append(String.format(
                        Locale.US,
                        "  - Self-ERLE & Dist    : %.1f dB | %.1f dB to silence\n",
                        row.selfErleDb,
                        row.distToFloorDb));
                out.append(String.format(
                        Locale.US,
                        "  - Envelope Corr       : Off=%.2f | On=%.2f\n\n",
                        row.corrAecOff,
                        row.corrAecOn));
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
                out.append(mSuiteCompleted
                        ? "AEC + RTL measurements not run (skipped or stopped).\n\n"
                        : "Waiting for AEC + RTL measurements...\n\n");
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
                if (mPoller == ReportPoller.this) {
                    mPoller = null;
                }
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
        File outFile;
        if (mReportFilePath != null && !mReportFilePath.isEmpty()) {
            File candidate = new File(mReportFilePath);
            if (candidate.isAbsolute() || dir == null) {
                outFile = candidate;
            } else {
                outFile = new File(dir, mReportFilePath);
            }
        } else {
            if (dir == null) {
                return;
            }
            outFile = new File(dir, REPORT_FILENAME);
        }
        if (outFile.exists()) {
            outFile.delete();
        }
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
                    || line.startsWith("SCORECARD BY STREAM")
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
                mScrollView.scrollTo(0, 0);
                mScrollView.post(() -> mScrollView.scrollTo(0, 0));
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

    static native boolean nativeIsMMapSupported();
    static native boolean nativeIsMMapExclusiveSupported();
    static native long nativeOpenOutputStream(int sampleRate, int channelCount);
    static native long nativeOpenInputStream(int sampleRate, int channelCount, int inputPreset);
    static native void nativeSetPlaybackData(long streamHandle, short[] audioData);
    static native void nativeSetPlaySpeech(long streamHandle, boolean playSpeech);
    static native int nativeGetPlaybackOffset(long streamHandle);
    static native int nativeStartStream(long streamHandle);
    static native int nativeStopStream(long streamHandle);
    static native void nativeCloseStream(long streamHandle);
    static native int nativeReadStream(
            long streamHandle, short[] buffer, int offset, int numSamples, long timeoutNanos);
    static native int nativeGetSessionId(long streamHandle);
    static native int nativeGetStreamState(long streamHandle);
    static native int nativeGetBurstFrames(long streamHandle);
    static native int nativeGetBufferCapacityFrames(long streamHandle);
    static native boolean nativeIsExclusive(long streamHandle);
    static native boolean nativeIsMMap(long streamHandle);
    static native long nativeGetTimestampPosition(long streamHandle);
    static native double nativeGetRoundTripTimestampLatencyMillis(
            long outStreamHandle, long inStreamHandle);
}
