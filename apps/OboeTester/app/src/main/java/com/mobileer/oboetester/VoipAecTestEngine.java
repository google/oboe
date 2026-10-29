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

import static com.mobileer.oboetester.VoipAecAnalyzer.AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION;
import static com.mobileer.oboetester.VoipAecAnalyzer.LEVEL_MIN_RMS_VAL;
import static com.mobileer.oboetester.VoipAecAnalyzer.MAX_VAL;
import static com.mobileer.oboetester.VoipAecAnalyzer.MIN_LIVE_SAMPLE_VARIANCE;
import static com.mobileer.oboetester.VoipAecAnalyzer.MIN_RMS_DB;
import static com.mobileer.oboetester.VoipAecAnalyzer.OBOE_ERROR_TIMEOUT;
import static com.mobileer.oboetester.VoipAecAnalyzer.PHASE_SILENCE_DURATION_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.PHASE_SILENCE_WARMUP_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.PHASE_SPEECH_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.PLAYER_RMS_WINDOW_SAMPLES;
import static com.mobileer.oboetester.VoipAecAnalyzer.SAMPLE_RATE;
import static com.mobileer.oboetester.VoipAecAnalyzer.SHOT_INTERVAL_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.SPEECH_LEVEL_WARMUP_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.STANDBY_PAUSE_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.STANDBY_POST_WARMUP_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.STANDBY_RESUME_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.STREAM_STATE_DISCONNECTED;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_AMBIENT_NOISE_CEILING_DBFS;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_CORR_AEC_OFF_MIN;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_CORR_AEC_ON_MAX;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_DIST_TO_FLOOR_DB;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_ERLE_DB;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_MIN_ERLE_AT_FLOOR_DB;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_STIMULUS_SNR_DB;
import static com.mobileer.oboetester.VoipAecAnalyzer.THRESHOLD_VOICE_AGENT_RTL_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.computeAcousticLatencyMs;
import static com.mobileer.oboetester.VoipAecAnalyzer.computeEnvelopeCorrelation;
import static com.mobileer.oboetester.VoipAecAnalyzer.computeWindowStats;
import static com.mobileer.oboetester.VoipAecAnalyzer.hasNonZeroSample;
import static com.mobileer.oboetester.VoipAecAnalyzer.rmsToDbfs;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;

import com.mobileer.oboetester.VoipAecAnalyzer.ApiRunMetrics;
import com.mobileer.oboetester.VoipAecAnalyzer.Finding;
import com.mobileer.oboetester.VoipAecAnalyzer.RmsEnvelopeTracker;
import com.mobileer.oboetester.VoipAecAnalyzer.SignalWindowStats;

import java.util.Arrays;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Reusable 5-Phase {@code VOICE_COMMUNICATION} stream execution engine for
 * {@link TestVoiceCommAecActivity}.
 *
 * <p>All 5 phases execute on a single {@code VOICE_COMMUNICATION} session (with speakerphone
 * playback on {@code USAGE_VOICE_COMMUNICATION}), eliminating cross-preset gain/microphone
 * differences so every dB delta ({@code Stimulus SNR}, {@code ERLE}, {@code Distance-to-Silence})
 * is 100% self-referenced on the exact same physical microphone and preamp routing path.
 */
final class VoipAecTestEngine {

    private static final String TAG = TestVoiceCommAecActivity.TAG;

    interface Callbacks {
        boolean isWorkerEnabled();
        void updateMetrics(Runnable updater);
        void onProgress(String progress);
        void onTelemetry(String line);
        void onPhaseStatus(ApiRunMetrics row, int phase, String status);
        void onPass();
        void onWarn();
        void onFail();
        void onFinding(Finding finding, String pathLabel, String evidence);
        void onCaptureRecorded(String apiLabel, short[] aecOff, int aecOffLen,
                short[] aecOn, int aecOnLen);
    }

    private VoipAecTestEngine() {}

    static void runSingleApiPath(
            ApiRunMetrics row,
            short[] speechPcm,
            int speechSampleRate,
            AudioManager am,
            boolean aecSupported,
            boolean nsSupported,
            boolean mmapSupported,
            boolean forceNsOff,
            boolean runAecPhases,
            boolean runStandby,
            Callbacks cb) {
        final boolean isNative = row.isNative;
        cb.onTelemetry("------------------------------------------------");
        cb.onTelemetry("STREAM: " + row.apiLabel);
        cb.onTelemetry("------------------------------------------------");

        VoipPlayer player = null;
        long nativeRec = 0;
        AudioRecord javaRec = null;
        AcousticEchoCanceler aec = null;
        NoiseSuppressor ns = null;

        try {
            player = new VoipPlayer(isNative, speechPcm);
            player.setPlaySpeech(false);
            if (!player.start()) {
                int failPhase = runAecPhases ? 1 : 5;
                cb.onPhaseStatus(row, failPhase, "FAIL (output open)");
                cb.onTelemetry("    FAIL: Could not open/start VoIP output player");
                emitStreamOpenOrStartFailure(
                        row.apiLabel, "output stream open/start failed", cb);
                cb.onFail();
                return;
            }

            int burst = 960;
            final int sessionId;
            if (isNative) {
                // Step 1: Open capture stream (do NOT start before attaching effects)
                nativeRec = TestVoiceCommAecActivity.nativeOpenInputStream(
                        SAMPLE_RATE, 1, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
                if (nativeRec == 0) {
                    int failPhase = runAecPhases ? 1 : 5;
                    cb.onPhaseStatus(row, failPhase, "FAIL (open failed)");
                    cb.onTelemetry("    FAIL: nativeOpenInputStream(VOICE_COMMUNICATION) failed");
                    emitStreamOpenOrStartFailure(
                            row.apiLabel,
                            "nativeOpenInputStream(VOICE_COMMUNICATION) returned 0",
                            cb);
                    cb.onFail();
                    return;
                }
                sessionId = TestVoiceCommAecActivity.nativeGetSessionId(nativeRec);
                final long inHandle = nativeRec;
                final long outHandle = player.getNativeHandle();
                final boolean inMmap = TestVoiceCommAecActivity.nativeIsMMap(inHandle);
                final boolean outMmap = TestVoiceCommAecActivity.nativeIsMMap(outHandle);
                final boolean inExcl = TestVoiceCommAecActivity.nativeIsExclusive(inHandle);
                final boolean outExcl = TestVoiceCommAecActivity.nativeIsExclusive(outHandle);
                final int outBurst = TestVoiceCommAecActivity.nativeGetBurstFrames(outHandle);
                final int outCap =
                        TestVoiceCommAecActivity.nativeGetBufferCapacityFrames(outHandle);
                final int inBurst = TestVoiceCommAecActivity.nativeGetBurstFrames(inHandle);
                final int inCap = TestVoiceCommAecActivity.nativeGetBufferCapacityFrames(inHandle);
                cb.updateMetrics(() -> {
                    row.sessionId = sessionId;
                    row.aecAvailable = aecSupported;
                    row.voipOutMmap = outMmap;
                    row.voipInMmap = inMmap;
                    row.voipOutExclusive = outExcl;
                    row.voipInExclusive = inExcl;
                    row.outBurstFrames = outBurst;
                    row.outCapacityFrames = outCap;
                    row.voipInBurstFrames = inBurst;
                    row.voipInCapacityFrames = inCap;
                    row.configSummary = String.format(
                            Locale.US,
                            "MMAP Out=%s In=%s, %df/%df",
                            outMmap ? "YES" : "NO",
                            inMmap ? "YES" : "NO",
                            outBurst,
                            inBurst);
                });
                burst = Math.max(256, inBurst);
            } else {
                int minBuf = Math.max(
                        16384,
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
                if (javaRec.getState() != AudioRecord.STATE_INITIALIZED) {
                    int failPhase = runAecPhases ? 1 : 5;
                    cb.onPhaseStatus(row, failPhase, "FAIL (init failed)");
                    cb.onTelemetry("    FAIL: AudioRecord(VOICE_COMMUNICATION) failed to init");
                    emitStreamOpenOrStartFailure(
                            row.apiLabel,
                            "AudioRecord(VOICE_COMMUNICATION) state=" + javaRec.getState(),
                            cb);
                    cb.onFail();
                    return;
                }
                sessionId = javaRec.getAudioSessionId();
                cb.updateMetrics(() -> {
                    row.sessionId = sessionId;
                    row.aecAvailable = aecSupported;
                    row.configSummary = "AudioTrack + AudioRecord VOIP";
                });
            }

            // Step 2: Pre-start effect attachment & strict initial disable verification
            boolean preStartCtrlFailed = false;
            if (aecSupported) {
                if (sessionId > 0) {
                    try {
                        aec = AcousticEchoCanceler.create(sessionId);
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to create AcousticEchoCanceler", e);
                    }
                }
                if (aec != null) {
                    final boolean defaultEnabled = aec.getEnabled();
                    final boolean hasControl = aec.hasControl();
                    final int rc = aec.setEnabled(false);
                    final boolean afterDisable = aec.getEnabled();
                    final boolean ctrlOk = (rc == AudioEffect.SUCCESS && !afterDisable);
                    cb.updateMetrics(() -> {
                        row.aecCreated = true;
                        row.aecDefaultEnabled = defaultEnabled;
                        row.aecHasControl = hasControl;
                        row.aecSetEnabledRc = rc;
                        row.aecControlVerified = ctrlOk;
                    });
                    cb.onTelemetry(String.format(
                            Locale.US,
                            "    AEC Attach  : session=%d, default=%b, hasCtrl=%b, setEnabled(false) rc=%d -> enabled=%b",
                            sessionId,
                            defaultEnabled,
                            hasControl,
                            rc,
                            afterDisable));
                    if (!ctrlOk) {
                        preStartCtrlFailed = true;
                        emitAecControlFailure(
                                row,
                                "pre-start setEnabled(false)",
                                false,
                                rc,
                                afterDisable,
                                hasControl,
                                cb);
                    }
                } else {
                    boolean inMmap = isNative
                            && TestVoiceCommAecActivity.nativeIsMMap(nativeRec);
                    cb.onTelemetry(String.format(
                            Locale.US,
                            "    AEC Attach  : FAILED (session=%d, aec=null, inMMap=%b)",
                            sessionId,
                            inMmap));
                    emitAecAttachFailure(row.apiLabel, sessionId, inMmap, cb);
                }
            } else {
                cb.onTelemetry(
                        "    AEC Attach  : SKIPPED (AcousticEchoCanceler.isAvailable() == false)");
            }

            if (nsSupported && sessionId > 0 && forceNsOff) {
                try {
                    ns = NoiseSuppressor.create(sessionId);
                    if (ns != null) {
                        int nsRc = ns.setEnabled(false);
                        boolean nsEn = ns.getEnabled();
                        cb.onTelemetry(String.format(
                                Locale.US,
                                "    NS Attach   : session=%d, setEnabled(false) rc=%d -> enabled=%b",
                                sessionId,
                                nsRc,
                                nsEn));
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to create NoiseSuppressor", e);
                }
            }

            // Step 3: Start capture stream AFTER effects are attached and configured
            if (isNative) {
                int startRc = TestVoiceCommAecActivity.nativeStartStream(nativeRec);
                if (startRc != 0) {
                    int failPhase = runAecPhases ? 1 : 5;
                    cb.onPhaseStatus(row, failPhase, "FAIL (start failed)");
                    cb.onTelemetry(
                            "    FAIL: nativeStartStream(VOICE_COMMUNICATION) returned " + startRc);
                    emitStreamOpenOrStartFailure(
                            row.apiLabel,
                            "nativeStartStream(VOICE_COMMUNICATION) returned " + startRc,
                            cb);
                    cb.onFail();
                    return;
                }
            } else {
                javaRec.startRecording();
                if (javaRec.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                    int failPhase = runAecPhases ? 1 : 5;
                    cb.onPhaseStatus(row, failPhase, "FAIL (start failed)");
                    cb.onTelemetry("    FAIL: AudioRecord.startRecording() did not enter RECORDING");
                    emitStreamOpenOrStartFailure(
                            row.apiLabel,
                            "AudioRecord recordingState=" + javaRec.getRecordingState(),
                            cb);
                    cb.onFail();
                    return;
                }
            }

            player.restartIfDisconnected();
            if (isNative) {
                final long outHandle = player.getNativeHandle();
                final boolean outMmap = TestVoiceCommAecActivity.nativeIsMMap(outHandle);
                final boolean outExcl = TestVoiceCommAecActivity.nativeIsExclusive(outHandle);
                final int outBurst = TestVoiceCommAecActivity.nativeGetBurstFrames(outHandle);
                final int outCap =
                        TestVoiceCommAecActivity.nativeGetBufferCapacityFrames(outHandle);
                cb.updateMetrics(() -> {
                    row.voipOutMmap = outMmap;
                    row.voipOutExclusive = outExcl;
                    row.outBurstFrames = outBurst;
                    row.outCapacityFrames = outCap;
                    row.configSummary = String.format(
                            Locale.US,
                            "MMAP Out=%s In=%s, %df/%df",
                            outMmap ? "YES" : "NO",
                            row.voipInMmap ? "YES" : "NO",
                            outBurst,
                            row.voipInBurstFrames);
                });
            }

            short[] ioBuf = new short[burst];
            long javaFramesReadComm = 0;

            if (runAecPhases) {
                // =====================================================================
                // Phase 1: Ambient Silence Floor (VOICE_COMMUNICATION, AEC OFF, Silence)
                // =====================================================================
                cb.onPhaseStatus(row, 1, "RUNNING...");
                cb.onProgress(row.apiLabel
                        + " - Phase 1/5: Ambient Silence Floor (VOICE_COMM, AEC OFF)...");
                cb.onTelemetry(
                        "  [Phase 1] Ambient Silence Floor (VOICE_COMMUNICATION, AEC OFF)");

                player.setPlaySpeech(false);
                boolean p1CtrlOk = !preStartCtrlFailed;
                if (aec != null && aec.getEnabled()) {
                    p1CtrlOk = verifyAecToggle(aec, false, "Phase 1 start", row, cb) && p1CtrlOk;
                }

                double p1SumSq = 0.0;
                long p1Samples = 0;
                int p1Blocks = 0;
                boolean p1HasNonZero = false;
                long p1StartMs = SystemClock.elapsedRealtime();
                long p1WarmupEnd = p1StartMs + PHASE_SILENCE_WARMUP_MS;
                long p1End = p1WarmupEnd + PHASE_SILENCE_DURATION_MS;

                while (SystemClock.elapsedRealtime() < p1End && cb.isWorkerEnabled()) {
                    int read = isNative
                            ? TestVoiceCommAecActivity.nativeReadStream(
                                    nativeRec, ioBuf, 0, burst, 100_000_000L)
                            : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                    if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                        cb.onTelemetry("    WARN: nativeReadStream returned " + read + " in Phase 1");
                        break;
                    } else if (!isNative && read < 0) {
                        cb.onTelemetry("    WARN: AudioRecord.read returned " + read + " in Phase 1");
                        break;
                    }
                    if (!isNative && read > 0) {
                        javaFramesReadComm += read;
                    }
                    if (read > 0 && SystemClock.elapsedRealtime() >= p1WarmupEnd) {
                        for (int i = 0; i < read; i++) {
                            short s = ioBuf[i];
                            if (s != 0) p1HasNonZero = true;
                            double v = s / MAX_VAL;
                            p1SumSq += v * v;
                        }
                        p1Samples += read;
                        p1Blocks++;
                    }
                }

                if (!cb.isWorkerEnabled()) return;

                final double avgNoiseRms = (p1Samples > 0)
                        ? Math.max(Math.sqrt(p1SumSq / p1Samples), LEVEL_MIN_RMS_VAL)
                        : LEVEL_MIN_RMS_VAL;
                final double p1Dbfs = rmsToDbfs(avgNoiseRms);
                cb.updateMetrics(() -> row.noiseFloorDbfs = p1Dbfs);

                boolean p1CaptureOk = (p1Samples > 0 && p1HasNonZero);
                boolean p1NoiseOk = (p1Dbfs <= THRESHOLD_AMBIENT_NOISE_CEILING_DBFS);
                boolean p1Pass = p1CtrlOk && p1CaptureOk && p1NoiseOk;
                String p1FailReason = !p1CtrlOk
                        ? "FAIL (AEC ctrl)"
                        : (!p1CaptureOk ? "FAIL (muted/dead mic)" : "FAIL (noisy room)");
                cb.onPhaseStatus(
                        row,
                        1,
                        String.format(
                                Locale.US,
                                "%s (%.1f dBFS)",
                                p1Pass ? "PASS" : p1FailReason,
                                p1Dbfs));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Silence (AEC OFF): %.1f dBFS (ceiling <= %.1f dBFS, live=%b, ctrl=%b) -> %s",
                        p1Dbfs,
                        THRESHOLD_AMBIENT_NOISE_CEILING_DBFS,
                        p1CaptureOk,
                        p1CtrlOk,
                        p1Pass ? "PASS" : "FAIL"));
                if (p1Pass) {
                    cb.onPass();
                } else {
                    cb.onFail();
                    if (!p1CaptureOk) {
                        Finding f = new Finding();
                        f.id = "PHASE1-CAPTURE-SILENT-OR-STALLED";
                        f.severity = "FAIL";
                        f.title = "Microphone capture returned 0 frames or all-zero digital silence in Phase 1";
                        f.whatHappened = "During Phase 1 on VOICE_COMMUNICATION (AEC OFF), the input stream either stalled or delivered only exact digital zeros (0x0000), indicating a muted or non-functional microphone path.";
                        f.androidContract = "Active audio input streams must deliver live ADC samples with non-zero thermal/quantization noise floor.";
                        f.whyItMatters = "A dead or muted microphone prevents voice capture in all communication and voice agent apps.";
                        f.howToFix = "Verify that hardware microphone privacy mute is off and that the VOICE_COMMUNICATION input route and ADC bias are enabled in the Audio HAL.";
                        cb.onFinding(
                                f,
                                row.apiLabel,
                                String.format(
                                        Locale.US,
                                        "%s: Phase 1 blocks=%d, samples=%d, nonZero=%b, ambient noise = %.1f dBFS.",
                                        row.apiLabel,
                                        p1Blocks,
                                        p1Samples,
                                        p1HasNonZero,
                                        p1Dbfs));
                    } else if (!p1NoiseOk) {
                        Finding f = new Finding();
                        f.id = "AMBIENT-NOISE-TOO-HIGH";
                        f.severity = "FAIL";
                        f.title = "Ambient room noise floor exceeds -35.0 dBFS ceiling";
                        f.whatHappened = "During Phase 1 silence capture on VOICE_COMMUNICATION (AEC OFF), the microphone recorded higher background noise than allowed for reliable echo cancellation measurement.";
                        f.androidContract = "Acoustic echo cancellation diagnostics require a quiet room (ambient noise <= -35.0 dBFS) so residual echo can be separated from room noise.";
                        f.whyItMatters = "High ambient noise masks echo attenuation (ERLE) and inflates microphone envelope variance.";
                        f.howToFix = "Re-run the test in a quiet room away from fans, HVAC vents, or conversation, and verify that the microphone hardware bias/ADC self-noise is below -40 dBFS.";
                        cb.onFinding(
                                f,
                                row.apiLabel,
                                String.format(
                                        Locale.US,
                                        "%s: Phase 1 ambient noise = %.1f dBFS (limit <= %.1f dBFS).",
                                        row.apiLabel,
                                        p1Dbfs,
                                        THRESHOLD_AMBIENT_NOISE_CEILING_DBFS));
                    }
                }

                // =====================================================================
                // Phase 2: Active AEC Silence Floor (VOICE_COMMUNICATION, AEC ON, Silence)
                // =====================================================================
                if (!aecSupported) {
                    cb.onPhaseStatus(row, 2, "SKIP (no AEC)");
                    cb.onTelemetry(
                            "  [Phase 2] SKIPPED (AcousticEchoCanceler not available on device)");
                } else if (aec == null) {
                    cb.onPhaseStatus(row, 2, "FAIL (AEC attach)");
                    cb.onTelemetry(
                            "  [Phase 2] FAIL: Cannot enable AEC (AcousticEchoCanceler failed to attach; see [AEC-ATTACH-FAILED])");
                    cb.onFail();
                } else {
                    cb.onPhaseStatus(row, 2, "RUNNING...");
                    cb.onProgress(row.apiLabel
                            + " - Phase 2/5: Active AEC Silence Floor (VOICE_COMM, AEC ON)...");
                    cb.onTelemetry(
                            "  [Phase 2] Active AEC Silence Floor (VOICE_COMMUNICATION, AEC ON)");

                    boolean p2CtrlOk = verifyAecToggle(aec, true, "Phase 2", row, cb);
                    player.restartIfDisconnected();

                    if (!p2CtrlOk) {
                        cb.onPhaseStatus(row, 2, "FAIL (AEC ctrl)");
                        cb.onFail();
                    } else {
                        double p2SumSq = 0.0;
                        long p2Samples = 0;
                        boolean p2HasNonZero = false;
                        long p2StartMs = SystemClock.elapsedRealtime();
                        long p2WarmupEnd = p2StartMs + PHASE_SILENCE_WARMUP_MS;
                        long p2End = p2WarmupEnd + PHASE_SILENCE_DURATION_MS;

                        while (SystemClock.elapsedRealtime() < p2End && cb.isWorkerEnabled()) {
                            int read = isNative
                                    ? TestVoiceCommAecActivity.nativeReadStream(
                                            nativeRec, ioBuf, 0, burst, 100_000_000L)
                                    : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                            if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                                cb.onTelemetry(
                                        "    WARN: nativeReadStream returned " + read
                                                + " in Phase 2");
                                break;
                            } else if (!isNative && read < 0) {
                                cb.onTelemetry(
                                        "    WARN: AudioRecord.read returned " + read
                                                + " in Phase 2");
                                break;
                            }
                            if (!isNative && read > 0) {
                                javaFramesReadComm += read;
                            }
                            if (read > 0 && SystemClock.elapsedRealtime() >= p2WarmupEnd) {
                                for (int i = 0; i < read; i++) {
                                    short s = ioBuf[i];
                                    if (s != 0) p2HasNonZero = true;
                                    double v = s / MAX_VAL;
                                    p2SumSq += v * v;
                                }
                                p2Samples += read;
                            }
                        }

                        if (!cb.isWorkerEnabled()) return;

                        final double avgAecOnSilRms = (p2Samples > 0)
                                ? Math.max(Math.sqrt(p2SumSq / p2Samples), LEVEL_MIN_RMS_VAL)
                                : LEVEL_MIN_RMS_VAL;
                        final double p2Dbfs = rmsToDbfs(avgAecOnSilRms);
                        cb.updateMetrics(() -> {
                            row.aecOnSilenceDbfs = p2Dbfs;
                            row.phase2Measured = true;
                        });

                        boolean p2CaptureOk = (p2Samples > 0 && p2HasNonZero);
                        boolean p2NoiseOk = (p2Dbfs <= THRESHOLD_AMBIENT_NOISE_CEILING_DBFS);
                        boolean p2Pass = p2CaptureOk && p2NoiseOk;
                        cb.onPhaseStatus(
                                row,
                                2,
                                String.format(
                                        Locale.US,
                                        "%s (%.1f dBFS)",
                                        p2Pass
                                                ? "PASS"
                                                : (!p2CaptureOk
                                                        ? "FAIL (muted/dead mic)"
                                                        : "FAIL"),
                                        p2Dbfs));
                        cb.onTelemetry(String.format(
                                Locale.US,
                                "    Silence (AEC ON) : %.1f dBFS (ceiling <= %.1f dBFS, live=%b) -> %s",
                                p2Dbfs,
                                THRESHOLD_AMBIENT_NOISE_CEILING_DBFS,
                                p2CaptureOk,
                                p2Pass ? "PASS" : "FAIL"));
                        if (p2Pass) {
                            cb.onPass();
                        } else {
                            cb.onFail();
                            Finding f = new Finding();
                            if (!p2CaptureOk) {
                                f.id = "PHASE2-CAPTURE-SILENT-OR-STALLED";
                                f.severity = "FAIL";
                                f.title = "Microphone capture stalled or muted when AEC was enabled in Phase 2";
                                f.whatHappened = "After enabling AcousticEchoCanceler during silence on VOICE_COMMUNICATION, the capture stream delivered 0 samples or all-zero digital silence.";
                                f.androidContract = "Enabling AcousticEchoCanceler must not mute or stall capture on VOICE_COMMUNICATION.";
                                f.whyItMatters = "Muting microphone capture when AEC is enabled breaks voice calls and voice agent input.";
                                f.howToFix = "Inspect the Audio HAL / voice DSP effect chain when AcousticEchoCanceler.setEnabled(true) is applied.";
                                cb.onFinding(
                                        f,
                                        row.apiLabel,
                                        String.format(
                                                Locale.US,
                                                "%s: Phase 2 samples=%d, nonZero=%b, level=%.1f dBFS.",
                                                row.apiLabel,
                                                p2Samples,
                                                p2HasNonZero,
                                                p2Dbfs));
                            } else {
                                f.id = "AEC-ON-SILENCE-NOISE-SPIKE";
                                f.severity = "FAIL";
                                f.title = "Active AEC silence floor exceeds -35.0 dBFS ceiling";
                                f.whatHappened = "Enabling AcousticEchoCanceler during digital silence playback on VOICE_COMMUNICATION caused the capture noise floor to exceed -35.0 dBFS.";
                                f.androidContract = "Enabling AcousticEchoCanceler on a VOICE_COMMUNICATION stream must not inject excessive comfort noise, gain boost, or DSP self-oscillation during silence.";
                                f.whyItMatters = "DSP noise injection during silence triggers false voice-activity detection (VAD) in conversational voice agents.";
                                f.howToFix = "Inspect the voice DSP comfort-noise generator (CNG) and AGC floor settings when AcousticEchoCanceler is enabled.";
                                cb.onFinding(
                                        f,
                                        row.apiLabel,
                                        String.format(
                                                Locale.US,
                                                "%s: Phase 2 AEC ON silence = %.1f dBFS vs Phase 1 AEC OFF silence = %.1f dBFS (limit <= %.1f dBFS).",
                                                row.apiLabel,
                                                p2Dbfs,
                                                p1Dbfs,
                                                THRESHOLD_AMBIENT_NOISE_CEILING_DBFS));
                            }
                        }
                    }
                }

                // =====================================================================
                // Phase 3: Uncancelled VoIP Echo & RTL (VOICE_COMMUNICATION, AEC OFF)
                // =====================================================================
                cb.onPhaseStatus(row, 3, "RUNNING...");
                cb.onProgress(row.apiLabel
                        + " - Phase 3/5: Uncancelled VoIP Echo & RTL (AEC OFF)...");
                cb.onTelemetry(
                        "  [Phase 3] Uncancelled VoIP Echo & RTL (VOICE_COMMUNICATION, AEC OFF)");

                boolean p3CtrlOk = true;
                if (aec != null) {
                    p3CtrlOk = verifyAecToggle(aec, false, "Phase 3", row, cb);
                }
                player.restartIfDisconnected();

                int maxShots = (PHASE_SPEECH_MS / SHOT_INTERVAL_MS) + 5;
                int maxSamples = (SAMPLE_RATE * PHASE_SPEECH_MS) / 1000;
                int warmupSamples = (SAMPLE_RATE * SPEECH_LEVEL_WARMUP_MS) / 1000;
                RmsEnvelopeTracker playerEnv3 = new RmsEnvelopeTracker(maxShots);
                RmsEnvelopeTracker recEnv3 = new RmsEnvelopeTracker(maxShots);
                player.setRmsTracker(playerEnv3);
                player.setPlaySpeech(false);

                long flushEnd3 = SystemClock.elapsedRealtime() + 300;
                while (SystemClock.elapsedRealtime() < flushEnd3 && cb.isWorkerEnabled()) {
                    if (isNative) {
                        int r = TestVoiceCommAecActivity.nativeReadStream(
                                nativeRec, ioBuf, 0, burst, 100_000_000L);
                        if (r < 0 && r != OBOE_ERROR_TIMEOUT) break;
                    } else {
                        int r = javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                        if (r < 0) break;
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
                int p3Blocks = 0;
                long p3StartMs = SystemClock.elapsedRealtime();
                long end3 = p3StartMs + PHASE_SPEECH_MS;
                long nextShot3 = p3StartMs + SHOT_INTERVAL_MS;

                while (SystemClock.elapsedRealtime() < end3 && cb.isWorkerEnabled()) {
                    int read = isNative
                            ? TestVoiceCommAecActivity.nativeReadStream(
                                    nativeRec, ioBuf, 0, burst, 100_000_000L)
                            : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                    if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                        cb.onTelemetry("    WARN: nativeReadStream returned " + read + " in Phase 3");
                        break;
                    } else if (!isNative && read < 0) {
                        cb.onTelemetry("    WARN: AudioRecord.read returned " + read + " in Phase 3");
                        break;
                    }
                    if (!isNative && read > 0) {
                        javaFramesReadComm += read;
                    }
                    if (read > 0) {
                        int toCopy = Math.min(read, recAecOff.length - recAecOffLen);
                        if (toCopy > 0) {
                            System.arraycopy(ioBuf, 0, recAecOff, recAecOffLen, toCopy);
                            recAecOffLen += toCopy;
                        }
                        recEnv3.updateDirect(ioBuf, read);
                        p3Blocks++;
                    }
                    if (SystemClock.elapsedRealtime() >= nextShot3) {
                        recEnv3.captureShot();
                        playerEnv3.captureShot();
                        nextShot3 += SHOT_INTERVAL_MS;
                        cb.onProgress(String.format(
                                Locale.US,
                                "%s - Phase 3/5 (AEC OFF): Rec %.1f dBFS | Play %.1f dBFS",
                                row.apiLabel,
                                recEnv3.getCurrentDbfs(),
                                playerEnv3.getCurrentDbfs()));
                    }
                }

                final double offTsRtlMs = isNative
                        ? TestVoiceCommAecActivity.nativeGetRoundTripTimestampLatencyMillis(
                                player.getNativeHandle(), nativeRec)
                        : computeJavaTimestampLatencyMillis(player, javaRec, javaFramesReadComm);

                player.setPlaySpeech(false);
                player.setRmsTracker(null);
                playerEnv3.setRunning(false);
                recEnv3.setRunning(false);

                if (!cb.isWorkerEnabled()) return;

                SignalWindowStats p3Stats = (recAecOffLen > warmupSamples)
                        ? computeWindowStats(recAecOff, warmupSamples, recAecOffLen)
                        : computeWindowStats(recAecOff, 0, recAecOffLen);
                final double p3Dbfs = p3Stats.dbfs;
                final double stimSnrDb = p3Dbfs - p1Dbfs;

                double corrBroadband = computeEnvelopeCorrelation(
                        playerEnv3.getSnapshotsCopy(), recEnv3.getSnapshotsCopy(), MIN_RMS_DB);
                double corrPreemph = computeEnvelopeCorrelation(
                        playerEnv3.getPreemphSnapshotsCopy(),
                        recEnv3.getSnapshotsCopy(),
                        MIN_RMS_DB);
                final double corrOff = Math.max(corrBroadband, corrPreemph);

                final double acousticRtlMs;
                if (recAecOffLen > 0) {
                    short[] slice = Arrays.copyOf(recAecOff, recAecOffLen);
                    acousticRtlMs = computeAcousticLatencyMs(
                            speechPcm, speechSampleRate, slice, SAMPLE_RATE, 500);
                } else {
                    acousticRtlMs = -1.0;
                }

                cb.updateMetrics(() -> {
                    row.voipOffTimestampRtlMs = offTsRtlMs;
                    row.aecOffDbfs = p3Dbfs;
                    row.stimulusSnrDb = stimSnrDb;
                    row.corrAecOff = corrOff;
                    row.voipOffAcousticRtlMs = acousticRtlMs;
                });

                // Drain any input frames buffered while computeAcousticLatencyMs executed
                while (cb.isWorkerEnabled()) {
                    int drained = isNative
                            ? TestVoiceCommAecActivity.nativeReadStream(
                                    nativeRec, ioBuf, 0, burst, 0L)
                            : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_NON_BLOCKING);
                    if (!isNative && drained > 0) {
                        javaFramesReadComm += drained;
                    }
                    if (drained <= 0) break;
                }

                boolean p3CaptureOk = (p3Stats.sampleCount > 0 && p3Stats.hasNonZero);
                boolean snrOk = (stimSnrDb >= THRESHOLD_STIMULUS_SNR_DB);
                boolean corrOffOk = (corrOff >= THRESHOLD_CORR_AEC_OFF_MIN);
                boolean p3Pass = p3CtrlOk && p3CaptureOk && snrOk && corrOffOk;
                if (p3Pass) {
                    cb.onPhaseStatus(
                            row,
                            3,
                            String.format(
                                    Locale.US,
                                    "PASS (SNR %.1fdB, C=%.2f, %.0fms)",
                                    stimSnrDb,
                                    corrOff,
                                    acousticRtlMs));
                    cb.onPass();
                } else {
                    cb.onPhaseStatus(
                            row,
                            3,
                            !p3CtrlOk
                                    ? "FAIL (AEC ctrl)"
                                    : String.format(
                                            Locale.US,
                                            "FAIL (SNR %.1fdB, C=%.2f)",
                                            stimSnrDb,
                                            corrOff));
                    cb.onFail();
                    if (!p3CaptureOk) {
                        Finding f = new Finding();
                        f.id = "PHASE3-CAPTURE-SILENT-OR-STALLED";
                        f.severity = "FAIL";
                        f.title = "Microphone capture stalled or returned all-zeros during Phase 3 playback";
                        f.whatHappened = "During Phase 3 speech playback on VOICE_COMMUNICATION (AEC OFF), capture returned 0 samples or all-zero digital silence.";
                        f.androidContract = "Full-duplex VOICE_COMMUNICATION streams must deliver live microphone samples while speakerphone playback is active.";
                        f.whyItMatters = "Full-duplex capture failure breaks simultaneous talk/listen in VoIP and voice agents.";
                        f.howToFix = "Inspect concurrent playback + capture routing on VOICE_COMMUNICATION in the Audio HAL.";
                        cb.onFinding(
                                f,
                                row.apiLabel,
                                String.format(
                                        Locale.US,
                                        "%s: Phase 3 blocks=%d, samples=%d, nonZero=%b.",
                                        row.apiLabel,
                                        p3Blocks,
                                        recAecOffLen,
                                        p3Stats.hasNonZero));
                    } else {
                        if (!snrOk) {
                            Finding f = new Finding();
                            f.id = "STIMULUS-SNR-TOO-LOW";
                            f.severity = "FAIL";
                            f.title = "Speaker-to-microphone acoustic coupling SNR is below 12.0 dB on VOICE_COMMUNICATION";
                            f.whatHappened = "During Phase 3 speech playback on VOICE_COMMUNICATION with AEC OFF, the captured loudspeaker level over [0.8s, 5.0s) was less than 12.0 dB above the Phase 1 VOICE_COMMUNICATION silence floor.";
                            f.androidContract = "Built-in speakerphone playback at maximum STREAM_VOICE_CALL volume must produce at least 12.0 dB SNR at the built-in microphone when AEC is disabled.";
                            f.whyItMatters = "Insufficient loudspeaker coupling or unreported always-on DSP AEC prevents verifying Echo Return Loss Enhancement (ERLE) and effect bypass control.";
                            f.howToFix = "Verify that STREAM_VOICE_CALL speakerphone routing and volume curves are active, nothing is obstructing the speaker or microphone ports, and AcousticEchoCanceler.setEnabled(false) bypasses hardware AEC.";
                            cb.onFinding(
                                    f,
                                    row.apiLabel,
                                    String.format(
                                            Locale.US,
                                            "%s: AEC OFF level = %.1f dBFS vs. Silence floor = %.1f dBFS (SNR = %.1f dB < 12.0 dB, blocks=%d).",
                                            row.apiLabel,
                                            p3Dbfs,
                                            p1Dbfs,
                                            stimSnrDb,
                                            p3Blocks));
                        }
                        if (!corrOffOk) {
                            Finding f = new Finding();
                            f.id = "AEC-BYPASS-IGNORED-OR-ALWAYS-ON";
                            f.severity = "FAIL";
                            f.title = "AcousticEchoCanceler.setEnabled(false) failed to bypass echo cancellation";
                            f.whatHappened = "During Phase 3 on VOICE_COMMUNICATION with AcousticEchoCanceler.setEnabled(false), the loudspeaker speech envelope was still suppressed (Correlation < 0.60).";
                            f.androidContract = "Android 17 CDD 5.4.4 [C-1-2] / [C-SR-1]: AcousticEchoCanceler.getEnabled() MUST reflect active/inactive state on non-watch devices, and controllability via AcousticEchoCanceler.setEnabled(false) is STRONGLY_RECOMMENDED to bypass echo cancellation.";
                            f.whyItMatters = "Apps and compliance tests cannot disable echo cancellation when needed, or the HAL is running unreported always-on DSP AEC.";
                            f.howToFix = "Ensure the Audio HAL effect implementation forwards setEnabled(false) to the voice DSP module and bypasses AEC adaptation/subtraction when disabled.";
                            cb.onFinding(
                                    f,
                                    row.apiLabel,
                                    String.format(
                                            Locale.US,
                                            "%s: Phase 3 AEC OFF Corr = %.2f (< 0.60), level = %.1f dBFS (SNR = %.1f dB, blocks=%d).",
                                            row.apiLabel,
                                            corrOff,
                                            p3Dbfs,
                                            stimSnrDb,
                                            p3Blocks));
                        }
                    }
                }

                cb.onTelemetry(String.format(
                        Locale.US,
                        "    AEC OFF Lvl : %.1f dBFS (Stimulus SNR = %.1f dB, Corr = %.2f)",
                        p3Dbfs,
                        stimSnrDb,
                        corrOff));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    VoIP RTL    : Acoustic = %.1f ms | HW TS = %s",
                        acousticRtlMs,
                        formatMs(offTsRtlMs)));

                // Flag/release-aware informational note when Native VoIP falls back from MMAP
                if (isNative && mmapSupported && (!row.voipOutMmap || !row.voipInMmap)) {
                    Finding f = new Finding();
                    f.id = "VOIP-MMAP-FALLBACK";
                    f.severity = "INFO";
                    f.androidContract = "MMAP on VOICE_COMMUNICATION requires both framework mmap_voip_aec support and HAL MMAP VoIP profiles with hardware DSP AEC bound by sessionId; partial or full fallback is expected when either direction uses a non-MMAP VoIP pipeline.";
                    if (!row.voipOutMmap && row.voipInMmap) {
                        f.title = "Native AAudio VOICE_COMMUNICATION used hybrid MMAP path (Out=Legacy, In=MMAP)";
                        f.whatHappened = "INPUT_PRESET_VOICE_COMMUNICATION capture opened on AAudio MMAP, while USAGE_VOICE_COMMUNICATION playback opened on the non-MMAP VoIP downlink (VOIP_RX / Legacy AudioFlinger) path so hardware DSP AEC receives the speaker echo reference.";
                        f.whyItMatters = "Capture benefits from zero-IPC MMAP bursts, while the non-MMAP VoIP playback pipeline governs total round-trip latency compared to full-duplex <= 100 ms MMAP VoIP.";
                        f.howToFix = "To achieve full-duplex MMAP VoIP (<= 100 ms RTL), also route USAGE_VOICE_COMMUNICATION playback to an MMAP profile that feeds the speaker echo reference into hardware DSP AEC.";
                    } else if (row.voipOutMmap) {
                        f.title = "Native AAudio VOICE_COMMUNICATION capture used Legacy (non-MMAP) path (Out=MMAP, In=Legacy)";
                        f.whatHappened = "USAGE_VOICE_COMMUNICATION playback opened on AAudio MMAP, while LowLatency INPUT_PRESET_VOICE_COMMUNICATION capture fell back to the Legacy AudioFlinger RecordThread path.";
                        f.whyItMatters = "Legacy AudioFlinger capture threads add extra buffering and Binder IPC overhead compared to full-duplex MMAP VoIP.";
                        f.howToFix = "On builds with mmap_voip_aec enabled, expose an MMAP capture profile for INPUT_PRESET_VOICE_COMMUNICATION in the Audio HAL with hardware DSP AEC attached via sessionId.";
                    } else {
                        f.title = "Native AAudio VOICE_COMMUNICATION used Legacy (non-MMAP) path";
                        f.whatHappened = "The device supports AAudio MMAP for general media/low-latency streams, while both LowLatency USAGE_VOICE_COMMUNICATION playback and INPUT_PRESET_VOICE_COMMUNICATION capture opened on the Legacy AudioFlinger path.";
                        f.whyItMatters = "Legacy AudioFlinger mixer/capture threads add ~20-60 ms of extra round-trip latency compared to MMAP VoIP.";
                        f.howToFix = "On builds with mmap_voip_aec enabled, expose MMAP profiles for USAGE_VOICE_COMMUNICATION and INPUT_PRESET_VOICE_COMMUNICATION in the Audio HAL with hardware DSP AEC attached via sessionId.";
                    }
                    cb.onFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s (SDK %d): Device MMAP=YES -> VoIP MMAP (Out=%s, In=%s).",
                                    row.apiLabel,
                                    Build.VERSION.SDK_INT,
                                    row.voipOutMmap ? "YES" : "NO",
                                    row.voipInMmap ? "YES" : "NO"));
                }

                // =====================================================================
                // Phase 4: Cancelled VoIP Echo & ERLE (VOICE_COMMUNICATION, AEC ON)
                // =====================================================================
                if (!aecSupported) {
                    cb.onPhaseStatus(row, 4, "SKIP (no AEC)");
                    cb.onTelemetry(
                            "  [Phase 4] SKIPPED (AcousticEchoCanceler not available on device)");
                    cb.onCaptureRecorded(row.apiLabel, recAecOff, recAecOffLen, null, 0);
                    cb.updateMetrics(() -> row.metricsPopulated = true);
                } else if (aec == null) {
                    cb.onPhaseStatus(row, 4, "FAIL (AEC attach)");
                    cb.onTelemetry(
                            "  [Phase 4] FAIL: Cannot measure active AEC ERLE because AcousticEchoCanceler failed to attach (see [AEC-ATTACH-FAILED])");
                    cb.onCaptureRecorded(row.apiLabel, recAecOff, recAecOffLen, null, 0);
                    cb.updateMetrics(() -> row.metricsPopulated = true);
                    cb.onFail();
                } else {
                    cb.onPhaseStatus(row, 4, "RUNNING...");
                    cb.onProgress(row.apiLabel
                            + " - Phase 4/5: Cancelled VoIP Echo & ERLE (AEC ON)...");
                    cb.onTelemetry(
                            "  [Phase 4] Cancelled VoIP Echo & ERLE (VOICE_COMMUNICATION, AEC ON)");

                    boolean p4CtrlOk = verifyAecToggle(aec, true, "Phase 4", row, cb);
                    player.restartIfDisconnected();

                    if (!p4CtrlOk) {
                        cb.onPhaseStatus(row, 4, "FAIL (AEC ctrl)");
                        cb.onCaptureRecorded(row.apiLabel, recAecOff, recAecOffLen, null, 0);
                        cb.updateMetrics(() -> row.metricsPopulated = true);
                        cb.onFail();
                    } else {
                        RmsEnvelopeTracker playerEnv4 = new RmsEnvelopeTracker(maxShots);
                        RmsEnvelopeTracker recEnv4 = new RmsEnvelopeTracker(maxShots);
                        player.setRmsTracker(playerEnv4);
                        player.setPlaySpeech(false);

                        long flushEnd4 = SystemClock.elapsedRealtime() + 300;
                        while (SystemClock.elapsedRealtime() < flushEnd4 && cb.isWorkerEnabled()) {
                            if (isNative) {
                                int r = TestVoiceCommAecActivity.nativeReadStream(
                                        nativeRec, ioBuf, 0, burst, 100_000_000L);
                                if (r < 0 && r != OBOE_ERROR_TIMEOUT) break;
                            } else {
                                int r = javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                                if (r < 0) break;
                                if (r > 0) javaFramesReadComm += r;
                            }
                        }

                        playerEnv4.reset();
                        recEnv4.reset();
                        playerEnv4.setRunning(true);
                        recEnv4.setRunning(true);
                        player.playSpeechFromStart();

                        short[] recAecOn = new short[maxSamples];
                        int recAecOnLen = 0;
                        int p4Blocks = 0;
                        long p4StartMs = SystemClock.elapsedRealtime();
                        long end4 = p4StartMs + PHASE_SPEECH_MS;
                        long nextShot4 = p4StartMs + SHOT_INTERVAL_MS;

                        while (SystemClock.elapsedRealtime() < end4 && cb.isWorkerEnabled()) {
                            int read = isNative
                                    ? TestVoiceCommAecActivity.nativeReadStream(
                                            nativeRec, ioBuf, 0, burst, 100_000_000L)
                                    : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                            if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                                cb.onTelemetry(
                                        "    WARN: nativeReadStream returned " + read
                                                + " in Phase 4");
                                break;
                            } else if (!isNative && read < 0) {
                                cb.onTelemetry(
                                        "    WARN: AudioRecord.read returned " + read
                                                + " in Phase 4");
                                break;
                            }
                            if (!isNative && read > 0) {
                                javaFramesReadComm += read;
                            }
                            if (read > 0) {
                                int toCopy = Math.min(read, recAecOn.length - recAecOnLen);
                                if (toCopy > 0) {
                                    System.arraycopy(ioBuf, 0, recAecOn, recAecOnLen, toCopy);
                                    recAecOnLen += toCopy;
                                }
                                recEnv4.updateDirect(ioBuf, read);
                                p4Blocks++;
                            }
                            if (SystemClock.elapsedRealtime() >= nextShot4) {
                                recEnv4.captureShot();
                                playerEnv4.captureShot();
                                nextShot4 += SHOT_INTERVAL_MS;
                                cb.onProgress(String.format(
                                        Locale.US,
                                        "%s - Phase 4/5 (AEC ON): Rec %.1f dBFS | Play %.1f dBFS",
                                        row.apiLabel,
                                        recEnv4.getCurrentDbfs(),
                                        playerEnv4.getCurrentDbfs()));
                            }
                        }

                        final double onTsRtlMs = isNative
                                ? TestVoiceCommAecActivity
                                        .nativeGetRoundTripTimestampLatencyMillis(
                                                player.getNativeHandle(), nativeRec)
                                : computeJavaTimestampLatencyMillis(
                                        player, javaRec, javaFramesReadComm);

                        player.setPlaySpeech(false);
                        player.setRmsTracker(null);
                        playerEnv4.setRunning(false);
                        recEnv4.setRunning(false);

                        cb.onCaptureRecorded(
                                row.apiLabel, recAecOff, recAecOffLen, recAecOn, recAecOnLen);

                        if (!cb.isWorkerEnabled()) return;

                        // Symmetric [0.8s, sharedEnd) window across Phase 3 & Phase 4
                        int sharedEnd = (recAecOffLen > warmupSamples
                                && recAecOnLen > warmupSamples)
                                ? Math.min(recAecOffLen, recAecOnLen)
                                : recAecOnLen;
                        SignalWindowStats p4Stats = (recAecOnLen > warmupSamples)
                                ? computeWindowStats(recAecOn, warmupSamples, sharedEnd)
                                : computeWindowStats(recAecOn, 0, recAecOnLen);
                        final double alignedOffDbfs =
                                (recAecOffLen > warmupSamples && sharedEnd > warmupSamples)
                                        ? computeWindowStats(recAecOff, warmupSamples, sharedEnd)
                                                .dbfs
                                        : p3Dbfs;
                        final double p4Dbfs = p4Stats.dbfs;
                        final double selfErle = alignedOffDbfs - p4Dbfs;
                        final double effectiveFloorDbfs = row.phase2Measured
                                ? Math.max(row.noiseFloorDbfs, row.aecOnSilenceDbfs)
                                : row.noiseFloorDbfs;
                        final double distToFloor = Math.max(0.0, p4Dbfs - effectiveFloorDbfs);

                        double silenceClampDb = Math.max(effectiveFloorDbfs + 3.0, -52.0);
                        final double corrOn = computeEnvelopeCorrelation(
                                playerEnv4.getSnapshotsCopy(),
                                recEnv4.getSnapshotsCopy(),
                                silenceClampDb);

                        cb.updateMetrics(() -> {
                            row.voipOnTimestampRtlMs = onTsRtlMs;
                            row.aecOffDbfs = alignedOffDbfs;
                            row.stimulusSnrDb = alignedOffDbfs - row.noiseFloorDbfs;
                            row.aecOnDbfs = p4Dbfs;
                            row.aecOnMeasured = true;
                            row.selfErleDb = selfErle;
                            row.distToFloorDb = distToFloor;
                            row.corrAecOn = corrOn;
                            row.metricsPopulated = true;
                        });

                        // Guard against muted/all-zero/frozen mic false-passing ERLE & correlation
                        boolean p4LiveOk = p4Stats.sampleCount > 0
                                && p4Stats.hasNonZero
                                && (p4Stats.sampleVariance > MIN_LIVE_SAMPLE_VARIANCE);
                        boolean erleOk = (selfErle >= THRESHOLD_ERLE_DB)
                                || (distToFloor <= THRESHOLD_DIST_TO_FLOOR_DB
                                        && selfErle >= THRESHOLD_MIN_ERLE_AT_FLOOR_DB);
                        boolean corrOnOk = (corrOn <= THRESHOLD_CORR_AEC_ON_MAX);
                        boolean p4Pass = p4LiveOk && erleOk && corrOnOk;

                        cb.onPhaseStatus(
                                row,
                                4,
                                !p4LiveOk
                                        ? "FAIL (muted/dead mic)"
                                        : String.format(
                                                Locale.US,
                                                "%s (ERLE %.1fdB, C=%.2f, %.0fms)",
                                                p4Pass ? "PASS" : "FAIL",
                                                selfErle,
                                                corrOn,
                                                onTsRtlMs));
                        cb.onTelemetry(String.format(
                                Locale.US,
                                "    AEC ON Lvl  : %.1f dBFS (Self-ERLE = %.1f dB, min >= %.1f dB, live=%b)",
                                p4Dbfs,
                                selfErle,
                                THRESHOLD_ERLE_DB,
                                p4LiveOk));
                        cb.onTelemetry(String.format(
                                Locale.US,
                                "    Residual    : Dist-to-Floor(max P1,P2) = %.1f dB | Corr = %.2f (<= %.2f)",
                                distToFloor,
                                corrOn,
                                THRESHOLD_CORR_AEC_ON_MAX));
                        cb.onTelemetry(String.format(
                                Locale.US,
                                "    AEC ON RTL  : HW TS = %s -> %s",
                                formatMs(onTsRtlMs),
                                p4Pass ? "PASS" : "FAIL"));

                        if (p4Pass) {
                            cb.onPass();
                        } else {
                            cb.onFail();
                            Finding f = new Finding();
                            if (!p4LiveOk) {
                                f.id = "PHASE4-CAPTURE-SILENT-OR-FROZEN";
                                f.severity = "FAIL";
                                f.title = "Microphone capture muted, stalled, or froze to constant samples in Phase 4";
                                f.whatHappened = "During Phase 4 speech playback with AEC ON, the captured microphone signal had zero variance or all-zero samples instead of a live residual noise floor.";
                                f.androidContract = "Active AEC must attenuate loudspeaker echo without muting or freezing the live microphone capture stream.";
                                f.whyItMatters = "A muted or frozen capture stream prevents user barge-in during voice agent playback and would otherwise masquerade as infinite ERLE.";
                                f.howToFix = "Ensure the HAL/DSP AEC and NLP stages preserve live microphone capture rather than outputting digital zeros during far-end speech.";
                                cb.onFinding(
                                        f,
                                        row.apiLabel,
                                        String.format(
                                                Locale.US,
                                                "%s: Phase 4 samples=%d, nonZero=%b, variance=%.3e (min > %.1e).",
                                                row.apiLabel,
                                                p4Stats.sampleCount,
                                                p4Stats.hasNonZero,
                                                p4Stats.sampleVariance,
                                                MIN_LIVE_SAMPLE_VARIANCE));
                            } else {
                                f.id = "AEC-INSUFFICIENT-CANCELLATION";
                                f.severity = "FAIL";
                                f.title = "Active AEC failed ERLE (>= 15.0 dB) or residual envelope correlation (<= 0.50)";
                                f.whatHappened = "With AcousticEchoCanceler.setEnabled(true) on VOICE_COMMUNICATION, loudspeaker speech bled into the captured microphone signal over the [0.8s, 5.0s) window.";
                                f.androidContract = "OboeTester / CTS-V diagnostic target: self-referenced ERLE >= 15.0 dB (or Dist-to-Floor <= 6.0 dB with ERLE >= 8.0 dB) and Residual Envelope Correlation <= 0.50 (CDD 5.4.4 correlation gate; dB/ERLE and Native AAudio checks are informational).";
                                f.whyItMatters = "Poor echo cancellation causes conversational voice agents to hear and transcribe their own TTS output (self-interruption) and causes echo in VoIP calls.";
                                f.howToFix = "1. Verify that the HAL feeds the speakerphone playback echo_reference into the hardware AEC module for both Legacy and MMAP (USAGE_VOICE_COMMUNICATION) output streams.\n  2. Check for sample-rate or DMA timestamp jumps between output and input streams that prevent the adaptive AEC filter from converging.";
                                cb.onFinding(
                                        f,
                                        row.apiLabel,
                                        String.format(
                                                Locale.US,
                                                "%s: Self-ERLE = %.1f dB (AEC OFF %.1f -> AEC ON %.1f dBFS), Dist-to-Floor = %.1f dB, Corr = %.2f (blocks=%d).",
                                                row.apiLabel,
                                                selfErle,
                                                alignedOffDbfs,
                                                p4Dbfs,
                                                distToFloor,
                                                corrOn,
                                                p4Blocks));
                            }
                        }
                    }
                }

                // Check 100ms Voice Agent Round-Trip Latency target on full-duplex Native MMAP
                if (isNative && row.voipOutMmap && row.voipInMmap) {
                    double worstFullDuplexRtlMs =
                            (row.voipOffAcousticRtlMs > 0 && row.voipOnTimestampRtlMs > 0)
                                    ? Math.max(row.voipOffAcousticRtlMs, row.voipOnTimestampRtlMs)
                                    : ((row.voipOffAcousticRtlMs > 0)
                                            ? row.voipOffAcousticRtlMs
                                            : row.voipOnTimestampRtlMs);
                    if (worstFullDuplexRtlMs > THRESHOLD_VOICE_AGENT_RTL_MS) {
                        cb.onWarn();
                        Finding f = new Finding();
                        f.id = "VOIP-RTL-EXCEEDS-100MS-TARGET";
                        f.severity = "WARNING";
                        f.title = "Full-duplex Native AAudio MMAP VoIP RTL exceeds 100.0 ms target";
                        f.whatHappened = "Measured round-trip latency (acoustic or hardware timestamp) on full-duplex Native AAudio MMAP VOICE_COMMUNICATION exceeded the 100.0 ms real-time budget.";
                        f.androidContract = "Full-duplex AAudio MMAP VOICE_COMMUNICATION (Out=MMAP, In=MMAP) targets <= 100.0 ms end-to-end round-trip audio latency.";
                        f.whyItMatters = "High VoIP path latency delays barge-in detection, degrades AEC filter convergence, and adds noticeable conversational lag.";
                        f.howToFix = "Ensure VOICE_COMMUNICATION MMAP profiles use small period/burst sizes (<= 5 ms) and avoid large DSP block-buffering queues in the HAL.";
                        cb.onFinding(
                                f,
                                row.apiLabel,
                                String.format(
                                        Locale.US,
                                        "%s (Out=MMAP, In=MMAP): VoIP Acoustic RTL = %.1f ms, VoIP HW Timestamp RTL = %s.",
                                        row.apiLabel,
                                        row.voipOffAcousticRtlMs,
                                        formatMs(row.voipOnTimestampRtlMs)));
                    }
                }
            }

            // =========================================================================
            // Phase 5: Multi-Turn Voice Agent Standby (3.5s Pause + Resume)
            // =========================================================================
            if (runStandby && cb.isWorkerEnabled()) {
                cb.onPhaseStatus(row, 5, "RUNNING...");
                cb.onProgress(row.apiLabel + " - Phase 5/5: 3.5s Turn Pause & Resume...");
                cb.onTelemetry("  [Phase 5] Multi-Turn Standby (3.5s Pause -> Resume)");

                boolean p5CtrlOk = true;
                if (aec != null) {
                    p5CtrlOk = verifyAecToggle(aec, true, "Phase 5 pre-standby", row, cb);
                }

                // Warm up for 250 ms before stopping
                long warmEnd = SystemClock.elapsedRealtime() + 250;
                while (SystemClock.elapsedRealtime() < warmEnd && cb.isWorkerEnabled()) {
                    if (isNative) {
                        int r = TestVoiceCommAecActivity.nativeReadStream(
                                nativeRec, ioBuf, 0, burst, 20_000_000L);
                        if (r < 0 && r != OBOE_ERROR_TIMEOUT) break;
                    } else {
                        int r = javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                        if (r < 0) break;
                    }
                }

                final long preTsPos;
                final long preOutFrames;
                if (isNative) {
                    preTsPos = TestVoiceCommAecActivity.nativeGetTimestampPosition(nativeRec);
                    preOutFrames = Math.max(
                            0L,
                            TestVoiceCommAecActivity.nativeGetFramesWritten(
                                    player.getNativeHandle()));
                    TestVoiceCommAecActivity.nativeStopStream(nativeRec);
                    player.pauseForStandby();
                } else {
                    AudioTimestamp ts = new AudioTimestamp();
                    preTsPos = (javaRec.getTimestamp(ts, AudioTimestamp.TIMEBASE_MONOTONIC)
                            == AudioRecord.SUCCESS)
                            ? ts.framePosition
                            : -1L;
                    preOutFrames = player.getFramesWritten();
                    javaRec.stop();
                    player.pauseForStandby();
                }

                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Sleeping %d ms (3.5s) to trigger HAL idle standby (preTs=%d)...",
                        STANDBY_PAUSE_MS,
                        preTsPos));
                if (!sleepInterruptible(STANDBY_PAUSE_MS, cb)) return;

                long resumeStartMs = SystemClock.elapsedRealtime();
                int startInRes = 0;
                int startOutRes = player.resumeAfterStandby();
                if (isNative) {
                    startInRes = TestVoiceCommAecActivity.nativeStartStream(nativeRec);
                } else {
                    javaRec.startRecording();
                    if (javaRec.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                        startInRes = -1;
                    }
                }
                player.playSpeechFromStart();

                long loopStartMs = SystemClock.elapsedRealtime();
                long postSteadyStartMs = loopStartMs + STANDBY_POST_WARMUP_MS;
                long firstReadMs = -1;
                boolean nonZeroVerified = false;
                boolean aecEnabledAfterWake =
                        !aecSupported || (aec != null && aec.getEnabled());
                double allPostSum = 0.0;
                double allPostSumSq = 0.0;
                long allPostSamples = 0;
                double steadyPostSumSq = 0.0;
                long steadyPostSamples = 0;
                int postBlocks = 0;
                long resumeEnd = loopStartMs + STANDBY_RESUME_MS;

                while (SystemClock.elapsedRealtime() < resumeEnd && cb.isWorkerEnabled()) {
                    int read = isNative
                            ? TestVoiceCommAecActivity.nativeReadStream(
                                    nativeRec, ioBuf, 0, burst, 100_000_000L)
                            : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                    if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                        cb.onTelemetry("    WARN: nativeReadStream returned " + read + " in Phase 5");
                        break;
                    } else if (!isNative && read < 0) {
                        cb.onTelemetry("    WARN: AudioRecord.read returned " + read + " in Phase 5");
                        break;
                    }
                    if (read > 0) {
                        long nowMs = SystemClock.elapsedRealtime();
                        long elapsed = nowMs - resumeStartMs;
                        if (firstReadMs < 0) {
                            firstReadMs = elapsed;
                            aecEnabledAfterWake =
                                    !aecSupported || (aec != null && aec.getEnabled());
                        }
                        if (!nonZeroVerified && hasNonZeroSample(ioBuf, read)) {
                            nonZeroVerified = true;
                        }
                        boolean inSteady = (nowMs >= postSteadyStartMs);
                        for (int i = 0; i < read; i++) {
                            double v = ioBuf[i] / MAX_VAL;
                            allPostSum += v;
                            allPostSumSq += v * v;
                            if (inSteady) {
                                steadyPostSumSq += v * v;
                            }
                        }
                        allPostSamples += read;
                        if (inSteady) {
                            steadyPostSamples += read;
                        }
                        postBlocks++;
                    }
                }

                final long postTsPos;
                final long postOutFrames;
                if (isNative) {
                    postTsPos = TestVoiceCommAecActivity.nativeGetTimestampPosition(nativeRec);
                    postOutFrames = Math.max(
                            0L,
                            TestVoiceCommAecActivity.nativeGetFramesWritten(
                                    player.getNativeHandle()));
                } else {
                    AudioTimestamp ts = new AudioTimestamp();
                    postTsPos = (javaRec.getTimestamp(ts, AudioTimestamp.TIMEBASE_MONOTONIC)
                            == AudioRecord.SUCCESS)
                            ? ts.framePosition
                            : -1L;
                    postOutFrames = player.getFramesWritten();
                }
                player.setPlaySpeech(false);

                if (!cb.isWorkerEnabled()) return;

                final long outFramesAdv = (postOutFrames >= preOutFrames)
                        ? (postOutFrames - preOutFrames)
                        : postOutFrames;
                double postRms = (steadyPostSamples > 0)
                        ? Math.max(Math.sqrt(steadyPostSumSq / steadyPostSamples),
                                LEVEL_MIN_RMS_VAL)
                        : ((allPostSamples > 0)
                                ? Math.max(Math.sqrt(allPostSumSq / allPostSamples),
                                        LEVEL_MIN_RMS_VAL)
                                : LEVEL_MIN_RMS_VAL);
                final double postDbfs = rmsToDbfs(postRms);
                double postMean = (allPostSamples > 0) ? (allPostSum / allPostSamples) : 0.0;
                double postVar = (allPostSamples > 0)
                        ? Math.max(0.0, (allPostSumSq / allPostSamples) - (postMean * postMean))
                        : 0.0;
                final boolean aecStillOn = p5CtrlOk
                        && aecEnabledAfterWake
                        && (!aecSupported || (aec != null && aec.getEnabled()));

                // Gate cross-standby monotonic position check on row.voipInMmap (Legacy/Java
                // AudioRecord resets framePosition on stop(), whereas MMAP preserves it).
                boolean tsValid;
                if (isNative && row.voipInMmap) {
                    tsValid = (postTsPos > 0)
                            && (preTsPos <= 0 || postTsPos > preTsPos);
                } else {
                    tsValid = (postTsPos > 0);
                }

                // Verify AEC is still acoustically suppressing speaker echo after standby wake
                boolean acousticResidualOk = true;
                if (aecSupported && aec != null && allPostSamples > 0) {
                    if (row.aecOnMeasured) {
                        double maxAllowedPostDbfs = Math.max(
                                row.aecOnDbfs + 8.0,
                                THRESHOLD_AMBIENT_NOISE_CEILING_DBFS);
                        acousticResidualOk = (postDbfs <= maxAllowedPostDbfs)
                                || ((row.aecOffDbfs - postDbfs) >= THRESHOLD_ERLE_DB);
                    } else {
                        acousticResidualOk = (postDbfs <= -22.0);
                    }
                }

                boolean livePostOk = nonZeroVerified && (postVar > MIN_LIVE_SAMPLE_VARIANCE);
                final boolean standbyPass = (startInRes == 0 && startOutRes == 0)
                        && (postBlocks > 0 && firstReadMs >= 0 && firstReadMs <= 600)
                        && (outFramesAdv > 0)
                        && livePostOk
                        && tsValid
                        && aecStillOn
                        && acousticResidualOk;

                final long finalFirstMs = firstReadMs;
                cb.updateMetrics(() -> {
                    row.standbyExecuted = true;
                    row.standbyPreTsPos = preTsPos;
                    row.standbyPostTsPos = postTsPos;
                    row.standbyOutFramesAdvanced = outFramesAdv;
                    row.standbyFirstFrameMs = finalFirstMs;
                    row.standbyPostRmsDbfs = postDbfs;
                    row.standbyAecStillEnabled = aecStillOn;
                    row.standbyPassed = standbyPass;
                });

                String tsDesc = (postTsPos > 0)
                        ? ((isNative && row.voipInMmap) ? "ts monotonic" : "ts active")
                        : "ts N/A";
                cb.onPhaseStatus(
                        row,
                        5,
                        standbyPass
                                ? String.format(
                                        Locale.US,
                                        "PASS (%dms, %s, %.1fdB)",
                                        firstReadMs,
                                        tsDesc,
                                        postDbfs)
                                : String.format(
                                        Locale.US,
                                        "FAIL (%dms, %.1fdB, ts %d->%d)",
                                        firstReadMs,
                                        postDbfs,
                                        preTsPos,
                                        postTsPos));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Post-Standby: %d ms | %.1f dBFS (echoOk=%b) | outAdv=%d | live=%b | ts=%d->%d | AEC=%s -> %s\n",
                        firstReadMs,
                        postDbfs,
                        acousticResidualOk,
                        outFramesAdv,
                        livePostOk,
                        preTsPos,
                        postTsPos,
                        aecStillOn ? "ON" : "LOST",
                        standbyPass ? "PASS" : "FAIL"));

                if (standbyPass) {
                    cb.onPass();
                } else {
                    cb.onFail();
                    Finding f = new Finding();
                    f.id = "VOIP-STANDBY-RESUME-FAILURE";
                    f.severity = "FAIL";
                    f.title = "VoIP stream failed to resume cleanly after 3.5s multi-turn standby pause";
                    f.whatHappened = "After stopping the VOICE_COMMUNICATION stream for 3.5 seconds (allowing the HAL to enter idle standby) and restarting it with speech playback, capture/playback stalled, returned dead samples, produced invalid timestamps, lost AcousticEchoCanceler state, or failed to suppress post-standby speaker echo.";
                    f.androidContract = "AAudio.h & StreamDescriptor.aidl: Full-duplex VoIP streams paused for >3 seconds between conversational turns must resume delivering frames immediately on start() with valid timestamps and active echo cancellation.";
                    f.whyItMatters = "Conversational AI voice agents pause audio streams between user turns; standby resume bugs cause frozen microphone capture or uncancelled self-echo on subsequent turns.";
                    f.howToFix = "1. Ensure the Audio HAL starts capture and playback DMA when transitioning from IDLE to ACTIVE ('burst') after standby exit.\n  2. Re-bind hardware AEC and echo_reference across standby transitions, and preserve cumulative frame counters on MMAP streams.";
                    cb.onFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: After 3.5s standby, firstFrame=%d ms, outAdv=%d, blocks=%d, livePcm=%b, postLevel=%.1f dBFS (echoOk=%b), ts=%d -> %d (mmapIn=%b), aecEnabled=%b.",
                                    row.apiLabel,
                                    firstReadMs,
                                    outFramesAdv,
                                    postBlocks,
                                    livePostOk,
                                    postDbfs,
                                    acousticResidualOk,
                                    preTsPos,
                                    postTsPos,
                                    row.voipInMmap,
                                    aecStillOn));
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Unexpected exception in runSingleApiPath (" + row.apiLabel + ")", e);
            cb.onTelemetry("    EXCEPTION in " + row.apiLabel + ": " + e.getMessage());
            if ("RUNNING...".equals(row.phase1Status)) cb.onPhaseStatus(row, 1, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase2Status)) cb.onPhaseStatus(row, 2, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase3Status)) cb.onPhaseStatus(row, 3, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase4Status)) cb.onPhaseStatus(row, 4, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase5Status)) cb.onPhaseStatus(row, 5, "FAIL (exception)");
            Finding f = new Finding();
            f.id = "VOIP-DIAGNOSTIC-RUNTIME-EXCEPTION";
            f.severity = "FAIL";
            f.title = "Unexpected exception while running VoIP & AEC diagnostic path";
            f.whatHappened = "An unhandled runtime exception aborted execution on " + row.apiLabel + ".";
            f.androidContract = "Audio stream and AudioEffect APIs must not throw unexpected runtime exceptions during valid lifecycle transitions.";
            f.whyItMatters = "Runtime exceptions crash or abort VoIP and voice agent sessions.";
            f.howToFix = "Inspect logcat stack trace for " + row.apiLabel + " exception details.";
            cb.onFinding(f, row.apiLabel, row.apiLabel + ": " + e.getClass().getSimpleName()
                    + ": " + e.getMessage());
            cb.onFail();
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
                TestVoiceCommAecActivity.nativeStopStream(nativeRec);
                TestVoiceCommAecActivity.nativeCloseStream(nativeRec);
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

    private static boolean verifyAecToggle(
            AcousticEchoCanceler aec,
            boolean targetEnabled,
            String stageLabel,
            ApiRunMetrics row,
            Callbacks cb) {
        int rc = Integer.MIN_VALUE;
        boolean actual = !targetEnabled;
        boolean hasControl = false;
        try {
            hasControl = aec.hasControl();
            rc = aec.setEnabled(targetEnabled);
            actual = aec.getEnabled();
        } catch (Exception e) {
            Log.e(TAG, "AEC setEnabled(" + targetEnabled + ") threw at " + stageLabel, e);
        }
        final int finalRc = rc;
        final boolean finalCtrl = hasControl;
        cb.updateMetrics(() -> {
            row.aecHasControl = finalCtrl;
            row.aecSetEnabledRc = finalRc;
        });
        if (rc == AudioEffect.SUCCESS && actual == targetEnabled) {
            return true;
        }
        cb.onTelemetry(String.format(
                Locale.US,
                "    FAIL: AEC setEnabled(%b) at %s failed (rc=%d, getEnabled()=%b, hasControl=%b)",
                targetEnabled,
                stageLabel,
                rc,
                actual,
                hasControl));
        emitAecControlFailure(row, stageLabel, targetEnabled, rc, actual, hasControl, cb);
        return false;
    }

    private static void emitAecAttachFailure(
            String apiLabel, int sessionId, boolean inMmap, Callbacks cb) {
        Finding f = new Finding();
        f.id = "AEC-ATTACH-FAILED";
        f.severity = "FAIL";
        f.title = "Failed to attach AcousticEchoCanceler to VOICE_COMMUNICATION session";
        f.whatHappened = "AcousticEchoCanceler.isAvailable() returned true, but AcousticEchoCanceler.create(sessionId) returned null (or sessionId <= 0) before starting capture.";
        f.androidContract = "When AcousticEchoCanceler.isAvailable() is true, apps opening a VOICE_COMMUNICATION input stream with an allocated audio session ID must be able to create and attach AcousticEchoCanceler.";
        f.whyItMatters = "Without an attached AcousticEchoCanceler (for example, when an MMAP input stream opens without hardware DSP AEC support), VoIP and voice agent streams suffer severe speaker echo.";
        f.howToFix = "1. Ensure AudioStreamBuilder / AudioRecord allocates and propagates a valid positive audio session ID.\n  2. On MMAP capture profiles (MmapCaptureThread), ensure hardware/DSP EFFECT_TYPE_AEC is registered for the device port in audio_effects.xml, or route VOICE_COMMUNICATION to a profile supporting AEC.";
        cb.onFinding(
                f,
                apiLabel,
                String.format(
                        Locale.US,
                        "%s: sessionId=%d, inMMap=%b, AcousticEchoCanceler.create(%d) == null.",
                        apiLabel,
                        sessionId,
                        inMmap,
                        sessionId));
    }

    private static void emitAecControlFailure(
            ApiRunMetrics row,
            String stageLabel,
            boolean expectedEnabled,
            int rc,
            boolean actualEnabled,
            boolean hasControl,
            Callbacks cb) {
        Finding f = new Finding();
        f.id = "AEC-CONTROL-FAILED";
        f.severity = "FAIL";
        f.title = "AcousticEchoCanceler.setEnabled() failed or lost effect control";
        f.whatHappened = "Calling AcousticEchoCanceler.setEnabled(" + expectedEnabled
                + ") did not return AudioEffect.SUCCESS (0) or getEnabled() did not reflect the requested state.";
        f.androidContract = "AcousticEchoCanceler created by the active capture client must retain control and honor setEnabled(false/true) transitions.";
        f.whyItMatters = "If another effect session steals control or the HAL rejects effect state changes, apps cannot reliably enable or bypass AEC.";
        f.howToFix = "Check AudioFlinger EffectModule priority/control ownership on sessionId and verify the HAL effect command EFFECT_CMD_ENABLE / EFFECT_CMD_DISABLE returns 0.";
        cb.onFinding(
                f,
                row.apiLabel,
                String.format(
                        Locale.US,
                        "%s (%s): session=%d, setEnabled(%b) -> rc=%d, getEnabled()=%b, hasControl=%b.",
                        row.apiLabel,
                        stageLabel,
                        row.sessionId,
                        expectedEnabled,
                        rc,
                        actualEnabled,
                        hasControl));
    }

    private static void emitStreamOpenOrStartFailure(
            String apiLabel, String detail, Callbacks cb) {
        Finding f = new Finding();
        f.id = "VOIP-STREAM-OPEN-OR-START-FAILURE";
        f.severity = "FAIL";
        f.title = "Failed to open or start full-duplex VOICE_COMMUNICATION stream";
        f.whatHappened = "Opening or starting the playback or capture stream on the VOICE_COMMUNICATION path failed before acoustic phases could execute.";
        f.androidContract = "Android devices with microphone and speaker support must open and start 48 kHz mono VOICE_COMMUNICATION playback and capture streams cleanly.";
        f.whyItMatters = "Stream open/start failures completely block VoIP calls and interactive voice agents.";
        f.howToFix = "Inspect AudioPolicyManager routing and Audio HAL stream open/start logs for USAGE_VOICE_COMMUNICATION and INPUT_PRESET_VOICE_COMMUNICATION.";
        cb.onFinding(f, apiLabel, apiLabel + ": " + detail);
    }

    static void evaluateCrossApiComparison(
            ApiRunMetrics first, ApiRunMetrics second, Callbacks cb) {
        ApiRunMetrics nat = first.isNative ? first : (second.isNative ? second : null);
        ApiRunMetrics jav = !first.isNative ? first : (!second.isNative ? second : null);
        if (nat == null || jav == null || !nat.metricsPopulated || !jav.metricsPopulated) {
            return;
        }
        cb.onTelemetry("------------------------------------------------");
        cb.onTelemetry("CROSS-API COMPARISON (NATIVE AAUDIO vs. JAVA)");
        cb.onTelemetry("------------------------------------------------");
        if (nat.voipOffAcousticRtlMs > 0 && jav.voipOffAcousticRtlMs > 0) {
            double deltaAcoustic = jav.voipOffAcousticRtlMs - nat.voipOffAcousticRtlMs;
            cb.onTelemetry(String.format(
                    Locale.US,
                    "  VoIP Acoustic : Nat=%.1fms | Java=%.1fms (%+.1fms)",
                    nat.voipOffAcousticRtlMs,
                    jav.voipOffAcousticRtlMs,
                    deltaAcoustic));
        }
        if (nat.voipOnTimestampRtlMs > 0 && jav.voipOnTimestampRtlMs > 0) {
            double deltaTs = jav.voipOnTimestampRtlMs - nat.voipOnTimestampRtlMs;
            cb.onTelemetry(String.format(
                    Locale.US,
                    "  VoIP HW TS    : Nat=%.1fms | Java=%.1fms (%+.1fms)",
                    nat.voipOnTimestampRtlMs,
                    jav.voipOnTimestampRtlMs,
                    deltaTs));
        }
        if (nat.aecOnMeasured && jav.aecOnMeasured) {
            cb.onTelemetry(String.format(
                    Locale.US,
                    "  VoIP Self-ERLE: Nat=%.1f dB | Java=%.1f dB\n",
                    nat.selfErleDb,
                    jav.selfErleDb));
        } else {
            cb.onTelemetry("");
        }
    }

    static String formatMs(double ms) {
        return (ms > 0.0) ? String.format(Locale.US, "%.1f ms", ms) : "N/A";
    }

    static boolean sleepInterruptible(int durationMs, Callbacks cb) {
        int remaining = durationMs;
        while (remaining > 0 && cb.isWorkerEnabled()) {
            int step = Math.min(25, remaining);
            SystemClock.sleep(step);
            remaining -= step;
        }
        return cb.isWorkerEnabled();
    }

    private static double computeJavaTimestampLatencyMillis(
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

    private static final class VoipPlayer {
        private final boolean mIsNative;
        private final short[] mAudioData;
        private final int mAudioDataLen;
        private final Object mNativeLock = new Object();
        private volatile long mNativeHandle = 0;
        private volatile AudioTrack mAudioTrack;
        private volatile boolean mPlaying = false;
        private volatile boolean mPlaySpeech = false;
        private volatile boolean mPausedJava = false;
        private volatile boolean mResetCursor = false;
        private final AtomicLong mFramesWritten = new AtomicLong(0);
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
            if (mIsNative) {
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        TestVoiceCommAecActivity.nativeSetPlaySpeech(mNativeHandle, play);
                    }
                }
            }
            mPlaySpeech = play;
            mResetCursor = true;
        }

        void playSpeechFromStart() {
            setPlaySpeech(true);
        }

        long getNativeHandle() {
            synchronized (mNativeLock) {
                return mNativeHandle;
            }
        }

        AudioTrack getAudioTrack() {
            return mAudioTrack;
        }

        long getFramesWritten() {
            return mFramesWritten.get();
        }

        void restartIfDisconnected() {
            if (!mIsNative) return;
            synchronized (mNativeLock) {
                if (mNativeHandle == 0) return;
                if (TestVoiceCommAecActivity.nativeGetStreamState(mNativeHandle)
                        == STREAM_STATE_DISCONNECTED) {
                    Log.w(TAG, "VoipPlayer output stream disconnected on route change; reopening");
                    TestVoiceCommAecActivity.nativeStopStream(mNativeHandle);
                    TestVoiceCommAecActivity.nativeCloseStream(mNativeHandle);
                    mNativeHandle = TestVoiceCommAecActivity.nativeOpenOutputStream(SAMPLE_RATE, 1);
                    if (mNativeHandle != 0) {
                        if (mAudioData != null) {
                            TestVoiceCommAecActivity.nativeSetPlaybackData(
                                    mNativeHandle, mAudioData);
                        }
                        TestVoiceCommAecActivity.nativeSetPlaySpeech(mNativeHandle, mPlaySpeech);
                        TestVoiceCommAecActivity.nativeStartStream(mNativeHandle);
                    }
                }
            }
        }

        void pauseForStandby() {
            if (mIsNative) {
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        TestVoiceCommAecActivity.nativeStopStream(mNativeHandle);
                    }
                }
            } else {
                mPausedJava = true;
                AudioTrack track = mAudioTrack;
                if (track != null) {
                    try {
                        track.pause();
                    } catch (Exception ignored) {
                    }
                }
            }
        }

        int resumeAfterStandby() {
            if (mIsNative) {
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        return TestVoiceCommAecActivity.nativeStartStream(mNativeHandle);
                    }
                    return -1;
                }
            } else {
                AudioTrack track = mAudioTrack;
                if (track == null) return -1;
                try {
                    track.play();
                } catch (Exception e) {
                    Log.e(TAG, "AudioTrack.play() failed after standby", e);
                    return -1;
                }
                mPausedJava = false;
                return (track.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) ? 0 : -1;
            }
        }

        boolean start() {
            if (mPlaying) return true;
            mFramesWritten.set(0);
            if (mIsNative) {
                synchronized (mNativeLock) {
                    mNativeHandle = TestVoiceCommAecActivity.nativeOpenOutputStream(SAMPLE_RATE, 1);
                    if (mNativeHandle == 0) return false;
                    if (mAudioData != null) {
                        TestVoiceCommAecActivity.nativeSetPlaybackData(mNativeHandle, mAudioData);
                    }
                    TestVoiceCommAecActivity.nativeSetPlaySpeech(mNativeHandle, mPlaySpeech);
                    if (TestVoiceCommAecActivity.nativeStartStream(mNativeHandle) != 0) {
                        TestVoiceCommAecActivity.nativeCloseStream(mNativeHandle);
                        mNativeHandle = 0;
                        return false;
                    }
                }
            } else {
                try {
                    int minBuf = AudioTrack.getMinBufferSize(
                            SAMPLE_RATE,
                            AudioFormat.CHANNEL_OUT_MONO,
                            AudioFormat.ENCODING_PCM_16BIT);
                    int bufferSize = Math.max(minBuf * 2, 7680);
                    AudioTrack.Builder builder = new AudioTrack.Builder()
                            .setAudioAttributes(new AudioAttributes.Builder()
                                    .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                                    .build())
                            .setAudioFormat(new AudioFormat.Builder()
                                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                    .setSampleRate(SAMPLE_RATE)
                                    .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                                    .build())
                            .setBufferSizeInBytes(bufferSize)
                            .setTransferMode(AudioTrack.MODE_STREAM);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        builder.setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY);
                    }
                    AudioTrack track = builder.build();
                    if (track.getState() != AudioTrack.STATE_INITIALIZED) {
                        track.release();
                        return false;
                    }
                    track.play();
                    mAudioTrack = track;
                } catch (Exception e) {
                    Log.e(TAG, "Failed to build/start VoIP AudioTrack", e);
                    return false;
                }
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

            if (mIsNative) {
                int lastOffset = 0;
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        lastOffset =
                                TestVoiceCommAecActivity.nativeGetPlaybackOffset(mNativeHandle);
                    }
                }
                while (mPlaying) {
                    if (mResetCursor) {
                        lastOffset = 0;
                        mResetCursor = false;
                    }
                    int currOffset;
                    synchronized (mNativeLock) {
                        if (mNativeHandle == 0) break;
                        currOffset =
                                TestVoiceCommAecActivity.nativeGetPlaybackOffset(mNativeHandle);
                    }
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
            } else {
                AudioTrack track = mAudioTrack;
                if (track == null) return;
                int offset = 0;
                while (mPlaying) {
                    if (mPausedJava) {
                        SystemClock.sleep(15);
                        continue;
                    }
                    if (mResetCursor) {
                        offset = 0;
                        mResetCursor = false;
                    }
                    short[] writeBuf;
                    if (mPlaySpeech && mAudioData != null && mAudioDataLen > 0) {
                        int toWrite = Math.min(burst, mAudioDataLen - offset);
                        System.arraycopy(mAudioData, offset, tempBuf, 0, toWrite);
                        if (toWrite < burst) {
                            System.arraycopy(mAudioData, 0, tempBuf, toWrite, burst - toWrite);
                            offset = burst - toWrite;
                        } else {
                            offset = (offset + toWrite) % mAudioDataLen;
                        }
                        writeBuf = tempBuf;
                        RmsEnvelopeTracker tracker = mRmsTracker;
                        if (tracker != null) {
                            tracker.updateDirect(writeBuf, burst);
                        }
                    } else {
                        writeBuf = silenceBuf;
                    }
                    int written;
                    try {
                        written = track.write(writeBuf, 0, burst, AudioTrack.WRITE_BLOCKING);
                    } catch (Exception e) {
                        if (mPlaying) {
                            Log.w(TAG, "AudioTrack.write threw in VoipPlayer", e);
                        }
                        break;
                    }
                    if (written < 0) {
                        if (mPlaying) {
                            Log.w(TAG, "AudioTrack.write returned error: " + written);
                        }
                        break;
                    }
                    mFramesWritten.addAndGet(written);
                }
            }
        }

        void stop() {
            mPlaying = false;
            mPausedJava = false;
            // Pause and stop Java AudioTrack BEFORE joining mThread so WRITE_BLOCKING unblocks!
            AudioTrack track = mAudioTrack;
            if (track != null) {
                try {
                    track.pause();
                    track.flush();
                    track.stop();
                } catch (Exception ignored) {
                }
            }
            if (mIsNative) {
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        TestVoiceCommAecActivity.nativeStopStream(mNativeHandle);
                    }
                }
            }
            if (mThread != null) {
                mThread.interrupt();
                boolean wasInterrupted = Thread.interrupted();
                try {
                    mThread.join(1000);
                } catch (InterruptedException ignored) {
                    wasInterrupted = true;
                } finally {
                    if (wasInterrupted) {
                        Thread.currentThread().interrupt();
                    }
                }
                mThread = null;
            }
            if (mIsNative) {
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        TestVoiceCommAecActivity.nativeCloseStream(mNativeHandle);
                        mNativeHandle = 0;
                    }
                }
            }
            if (track != null) {
                mAudioTrack = null;
                try {
                    track.release();
                } catch (Exception ignored) {
                }
            }
        }
    }
}
