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
import static com.mobileer.oboetester.VoipAecAnalyzer.MIN_RMS_DB;
import static com.mobileer.oboetester.VoipAecAnalyzer.MIN_RMS_VAL;
import static com.mobileer.oboetester.VoipAecAnalyzer.OBOE_ERROR_TIMEOUT;
import static com.mobileer.oboetester.VoipAecAnalyzer.PHASE_SILENCE_DURATION_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.PHASE_SILENCE_WARMUP_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.PHASE_SPEECH_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.PLAYER_RMS_WINDOW_SAMPLES;
import static com.mobileer.oboetester.VoipAecAnalyzer.SAMPLE_RATE;
import static com.mobileer.oboetester.VoipAecAnalyzer.SHOT_INTERVAL_MS;
import static com.mobileer.oboetester.VoipAecAnalyzer.STANDBY_PAUSE_MS;
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
import static com.mobileer.oboetester.VoipAecAnalyzer.computeBlockRms;
import static com.mobileer.oboetester.VoipAecAnalyzer.computeEnvelopeCorrelation;
import static com.mobileer.oboetester.VoipAecAnalyzer.hasNonZeroSample;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;

import com.mobileer.oboetester.VoipAecAnalyzer.ApiRunMetrics;
import com.mobileer.oboetester.VoipAecAnalyzer.Finding;
import com.mobileer.oboetester.VoipAecAnalyzer.RmsEnvelopeTracker;

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
                cb.onPhaseStatus(row, 1, "FAIL (output open)");
                cb.onTelemetry("    FAIL: Could not start VoIP output player");
                cb.onFail();
                return;
            }

            int burst = 960;
            int sessionId = -1;
            if (isNative) {
                nativeRec = TestVoiceCommAecActivity.nativeOpenInputStream(
                        SAMPLE_RATE, 1, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
                if (nativeRec == 0) {
                    cb.onPhaseStatus(row, 1, "FAIL (open failed)");
                    cb.onTelemetry("    FAIL: nativeOpenInputStream(VOICE_COMMUNICATION) failed");
                    cb.onFail();
                    return;
                }
                if (TestVoiceCommAecActivity.nativeStartStream(nativeRec) != 0) {
                    cb.onPhaseStatus(row, 1, "FAIL (start failed)");
                    cb.onTelemetry("    FAIL: nativeStartStream(VOICE_COMMUNICATION) failed");
                    cb.onFail();
                    return;
                }
                sessionId = TestVoiceCommAecActivity.nativeGetSessionId(nativeRec);
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
                    cb.onPhaseStatus(row, 1, "FAIL (init failed)");
                    cb.onTelemetry("    FAIL: AudioRecord(VOICE_COMMUNICATION) failed to init");
                    cb.onFail();
                    return;
                }
                javaRec.startRecording();
                sessionId = javaRec.getAudioSessionId();
                row.configSummary = "AudioTrack + AudioRecord VOIP";
            }

            if (aecSupported && sessionId > 0) {
                try {
                    aec = AcousticEchoCanceler.create(sessionId);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to create AcousticEchoCanceler", e);
                }
            }
            if (nsSupported && sessionId > 0 && forceNsOff) {
                try {
                    ns = NoiseSuppressor.create(sessionId);
                    if (ns != null) {
                        ns.setEnabled(false);
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to create NoiseSuppressor", e);
                }
            }
            if (aec != null) {
                try {
                    aec.setEnabled(false);
                } catch (Exception ignored) {
                }
            }
            player.restartIfDisconnected();

            if (isNative) {
                row.voipOutMmap = TestVoiceCommAecActivity.nativeIsMMap(player.getNativeHandle());
                row.voipInMmap = TestVoiceCommAecActivity.nativeIsMMap(nativeRec);
                row.voipOutExclusive =
                        TestVoiceCommAecActivity.nativeIsExclusive(player.getNativeHandle());
                row.voipInExclusive = TestVoiceCommAecActivity.nativeIsExclusive(nativeRec);
                row.outBurstFrames =
                        TestVoiceCommAecActivity.nativeGetBurstFrames(player.getNativeHandle());
                row.outCapacityFrames = TestVoiceCommAecActivity.nativeGetBufferCapacityFrames(
                        player.getNativeHandle());
                row.voipInBurstFrames = TestVoiceCommAecActivity.nativeGetBurstFrames(nativeRec);
                row.voipInCapacityFrames =
                        TestVoiceCommAecActivity.nativeGetBufferCapacityFrames(nativeRec);
                row.configSummary = String.format(
                        Locale.US,
                        "MMAP Out=%s In=%s, %df/%df",
                        row.voipOutMmap ? "YES" : "NO",
                        row.voipInMmap ? "YES" : "NO",
                        row.outBurstFrames,
                        row.voipInBurstFrames);
                burst = Math.max(256, row.voipInBurstFrames);
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
                if (aec != null) {
                    try {
                        aec.setEnabled(false);
                    } catch (Exception ignored) {
                    }
                }

                double p1RmsSum = 0.0;
                int p1Count = 0;
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
                        p1RmsSum += computeBlockRms(ioBuf, read);
                        p1Count++;
                        if (!p1HasNonZero && hasNonZeroSample(ioBuf, read)) {
                            p1HasNonZero = true;
                        }
                    }
                }

                if (!cb.isWorkerEnabled()) return;

                double avgNoiseRms = (p1Count > 0) ? (p1RmsSum / p1Count) : MIN_RMS_VAL;
                row.noiseFloorDbfs = 20.0 * Math.log10(Math.max(avgNoiseRms, MIN_RMS_VAL));
                boolean p1CaptureOk = (p1Count > 0 && p1HasNonZero);
                boolean p1Pass = p1CaptureOk
                        && (row.noiseFloorDbfs <= THRESHOLD_AMBIENT_NOISE_CEILING_DBFS);
                String p1FailReason = !p1CaptureOk ? "FAIL (muted/dead mic)" : "FAIL (noisy room)";
                cb.onPhaseStatus(
                        row,
                        1,
                        String.format(
                                Locale.US,
                                "%s (%.1f dBFS)",
                                p1Pass ? "PASS" : p1FailReason,
                                row.noiseFloorDbfs));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Silence (AEC OFF): %.1f dBFS (ceiling <= %.1f dBFS, live=%b) -> %s",
                        row.noiseFloorDbfs,
                        THRESHOLD_AMBIENT_NOISE_CEILING_DBFS,
                        p1CaptureOk,
                        p1Pass ? "PASS" : "FAIL"));
                if (p1Pass) {
                    cb.onPass();
                } else {
                    cb.onFail();
                    Finding f = new Finding();
                    if (!p1CaptureOk) {
                        f.id = "PHASE1-CAPTURE-SILENT-OR-STALLED";
                        f.severity = "FAIL";
                        f.title = "Microphone capture returned 0 frames or all-zero digital silence in Phase 1";
                        f.whatHappened = "During Phase 1 on VOICE_COMMUNICATION (AEC OFF), the input stream either stalled or delivered only exact digital zeros (0x0000), indicating a muted or non-functional microphone path.";
                        f.androidContract = "Active audio input streams must deliver live ADC samples with non-zero thermal/quantization noise floor.";
                        f.whyItMatters = "A dead or muted microphone prevents voice capture in all communication and voice agent apps.";
                        f.howToFix = "Verify that hardware microphone privacy mute is off and that the VOICE_COMMUNICATION input route and ADC bias are enabled in the Audio HAL.";
                    } else {
                        f.id = "AMBIENT-NOISE-TOO-HIGH";
                        f.severity = "FAIL";
                        f.title = "Ambient room noise floor exceeds -35.0 dBFS ceiling";
                        f.whatHappened = "During Phase 1 silence capture on VOICE_COMMUNICATION (AEC OFF), the microphone recorded higher background noise than allowed for reliable echo cancellation measurement.";
                        f.androidContract = "Acoustic echo cancellation diagnostics require a quiet room (ambient noise <= -35.0 dBFS) so residual echo can be separated from room noise.";
                        f.whyItMatters = "High ambient noise masks echo attenuation (ERLE) and inflates microphone envelope variance.";
                        f.howToFix = "Re-run the test in a quiet room away from fans, HVAC vents, or conversation, and verify that the microphone hardware bias/ADC self-noise is below -40 dBFS.";
                    }
                    cb.onFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Phase 1 blocks=%d, nonZero=%b, ambient noise = %.1f dBFS (limit <= %.1f dBFS).",
                                    row.apiLabel,
                                    p1Count,
                                    p1HasNonZero,
                                    row.noiseFloorDbfs,
                                    THRESHOLD_AMBIENT_NOISE_CEILING_DBFS));
                }

                // =====================================================================
                // Phase 2: Active AEC Silence Floor (VOICE_COMMUNICATION, AEC ON, Silence)
                // =====================================================================
                cb.onPhaseStatus(row, 2, "RUNNING...");
                cb.onProgress(row.apiLabel
                        + " - Phase 2/5: Active AEC Silence Floor (VOICE_COMM, AEC ON)...");
                cb.onTelemetry(
                        "  [Phase 2] Active AEC Silence Floor (VOICE_COMMUNICATION, AEC ON)");

                if (aec != null) {
                    try {
                        aec.setEnabled(true);
                    } catch (Exception ignored) {
                    }
                }
                player.restartIfDisconnected();

                double p2RmsSum = 0.0;
                int p2Count = 0;
                long p2StartMs = SystemClock.elapsedRealtime();
                long p2WarmupEnd = p2StartMs + PHASE_SILENCE_WARMUP_MS;
                long p2End = p2WarmupEnd + PHASE_SILENCE_DURATION_MS;

                while (SystemClock.elapsedRealtime() < p2End && cb.isWorkerEnabled()) {
                    int read = isNative
                            ? TestVoiceCommAecActivity.nativeReadStream(
                                    nativeRec, ioBuf, 0, burst, 100_000_000L)
                            : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                    if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                        cb.onTelemetry("    WARN: nativeReadStream returned " + read + " in Phase 2");
                        break;
                    } else if (!isNative && read < 0) {
                        cb.onTelemetry("    WARN: AudioRecord.read returned " + read + " in Phase 2");
                        break;
                    }
                    if (!isNative && read > 0) {
                        javaFramesReadComm += read;
                    }
                    if (read > 0 && SystemClock.elapsedRealtime() >= p2WarmupEnd) {
                        p2RmsSum += computeBlockRms(ioBuf, read);
                        p2Count++;
                    }
                }

                if (!cb.isWorkerEnabled()) return;

                double avgAecOnSilRms = (p2Count > 0) ? (p2RmsSum / p2Count) : MIN_RMS_VAL;
                row.aecOnSilenceDbfs = 20.0 * Math.log10(Math.max(avgAecOnSilRms, MIN_RMS_VAL));
                boolean p2Pass = (p2Count > 0)
                        && (row.aecOnSilenceDbfs <= THRESHOLD_AMBIENT_NOISE_CEILING_DBFS);
                cb.onPhaseStatus(
                        row,
                        2,
                        String.format(
                                Locale.US,
                                "%s (%.1f dBFS)",
                                p2Pass ? "PASS" : "FAIL",
                                row.aecOnSilenceDbfs));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Silence (AEC ON) : %.1f dBFS (ceiling <= %.1f dBFS) -> %s",
                        row.aecOnSilenceDbfs,
                        THRESHOLD_AMBIENT_NOISE_CEILING_DBFS,
                        p2Pass ? "PASS" : "FAIL"));
                if (p2Pass) {
                    cb.onPass();
                } else {
                    cb.onFail();
                    Finding f = new Finding();
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
                                    row.aecOnSilenceDbfs,
                                    row.noiseFloorDbfs,
                                    THRESHOLD_AMBIENT_NOISE_CEILING_DBFS));
                }

                // =====================================================================
                // Phase 3: Uncancelled VoIP Echo & RTL (VOICE_COMMUNICATION, AEC OFF)
                // =====================================================================
                cb.onPhaseStatus(row, 3, "RUNNING...");
                cb.onProgress(row.apiLabel
                        + " - Phase 3/5: Uncancelled VoIP Echo & RTL (AEC OFF)...");
                cb.onTelemetry(
                        "  [Phase 3] Uncancelled VoIP Echo & RTL (VOICE_COMMUNICATION, AEC OFF)");

                if (aec != null) {
                    try {
                        aec.setEnabled(false);
                    } catch (Exception ignored) {
                    }
                }
                player.restartIfDisconnected();

                int maxShots = (PHASE_SPEECH_MS / SHOT_INTERVAL_MS) + 5;
                int maxSamples = (SAMPLE_RATE * PHASE_SPEECH_MS) / 1000;
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
                double p3RmsSum = 0.0;
                int p3Count = 0;
                long end3 = SystemClock.elapsedRealtime() + PHASE_SPEECH_MS;
                long nextShot3 = SystemClock.elapsedRealtime() + SHOT_INTERVAL_MS;

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
                        p3RmsSum += computeBlockRms(ioBuf, read);
                        p3Count++;
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

                if (isNative) {
                    row.voipOffTimestampRtlMs =
                            TestVoiceCommAecActivity.nativeGetRoundTripTimestampLatencyMillis(
                                    player.getNativeHandle(), nativeRec);
                } else {
                    row.voipOffTimestampRtlMs = computeJavaTimestampLatencyMillis(
                            player, javaRec, javaFramesReadComm);
                }

                player.setPlaySpeech(false);
                player.setRmsTracker(null);
                playerEnv3.setRunning(false);
                recEnv3.setRunning(false);

                if (!cb.isWorkerEnabled()) return;

                double avgOffRms = (p3Count > 0) ? (p3RmsSum / p3Count) : MIN_RMS_VAL;
                row.aecOffDbfs = 20.0 * Math.log10(Math.max(avgOffRms, MIN_RMS_VAL));
                row.stimulusSnrDb = row.aecOffDbfs - row.noiseFloorDbfs;

                double corrBroadband = computeEnvelopeCorrelation(
                        playerEnv3.getSnapshotsCopy(), recEnv3.getSnapshotsCopy(), MIN_RMS_DB);
                double corrPreemph = computeEnvelopeCorrelation(
                        playerEnv3.getPreemphSnapshotsCopy(),
                        recEnv3.getSnapshotsCopy(),
                        MIN_RMS_DB);
                row.corrAecOff = Math.max(corrBroadband, corrPreemph);

                if (recAecOffLen > 0) {
                    short[] slice = Arrays.copyOf(recAecOff, recAecOffLen);
                    row.voipOffAcousticRtlMs = computeAcousticLatencyMs(
                            speechPcm, speechSampleRate, slice, SAMPLE_RATE, 500);
                }

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

                boolean snrOk = (row.stimulusSnrDb >= THRESHOLD_STIMULUS_SNR_DB);
                boolean corrOffOk = (row.corrAecOff >= THRESHOLD_CORR_AEC_OFF_MIN);
                boolean p3Pass = (p3Count > 0 && recAecOffLen > 0) && snrOk && corrOffOk;
                if (p3Pass) {
                    cb.onPhaseStatus(
                            row,
                            3,
                            String.format(
                                    Locale.US,
                                    "PASS (SNR %.1fdB, C=%.2f, %.0fms)",
                                    row.stimulusSnrDb,
                                    row.corrAecOff,
                                    row.voipOffAcousticRtlMs));
                    cb.onPass();
                } else {
                    cb.onPhaseStatus(
                            row,
                            3,
                            String.format(
                                    Locale.US,
                                    "FAIL (SNR %.1fdB, C=%.2f)",
                                    row.stimulusSnrDb,
                                    row.corrAecOff));
                    cb.onFail();
                    if (!snrOk) {
                        Finding f = new Finding();
                        f.id = "STIMULUS-SNR-TOO-LOW";
                        f.severity = "FAIL";
                        f.title = "Speaker-to-microphone acoustic coupling SNR is below 12.0 dB on VOICE_COMMUNICATION";
                        f.whatHappened = "During Phase 3 speech playback on VOICE_COMMUNICATION with AEC OFF, the captured loudspeaker level was less than 12.0 dB above the Phase 1 VOICE_COMMUNICATION silence floor.";
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
                                        row.aecOffDbfs,
                                        row.noiseFloorDbfs,
                                        row.stimulusSnrDb,
                                        p3Count));
                    }
                    if (!corrOffOk) {
                        Finding f = new Finding();
                        f.id = "AEC-BYPASS-IGNORED-OR-ALWAYS-ON";
                        f.severity = "FAIL";
                        f.title = "AcousticEchoCanceler.setEnabled(false) failed to bypass echo cancellation";
                        f.whatHappened = "During Phase 3 on VOICE_COMMUNICATION with AcousticEchoCanceler.setEnabled(false), the loudspeaker speech envelope was still suppressed (Correlation < 0.60).";
                        f.androidContract = "CDD 5.4 / AcousticEchoCanceler API: Disabling AcousticEchoCanceler via setEnabled(false) on a VOICE_COMMUNICATION stream must disable echo cancellation in the HAL/DSP.";
                        f.whyItMatters = "Apps and compliance tests cannot disable echo cancellation when needed, or the HAL is running unreported always-on DSP AEC.";
                        f.howToFix = "Ensure the Audio HAL effect implementation forwards setEnabled(false) to the voice DSP module and bypasses AEC adaptation/subtraction when disabled.";
                        cb.onFinding(
                                f,
                                row.apiLabel,
                                String.format(
                                        Locale.US,
                                        "%s: Phase 3 AEC OFF Corr = %.2f (< 0.60), level = %.1f dBFS (SNR = %.1f dB, blocks=%d).",
                                        row.apiLabel,
                                        row.corrAecOff,
                                        row.aecOffDbfs,
                                        row.stimulusSnrDb,
                                        p3Count));
                    }
                }

                cb.onTelemetry(String.format(
                        Locale.US,
                        "    AEC OFF Lvl : %.1f dBFS (Stimulus SNR = %.1f dB, Corr = %.2f)",
                        row.aecOffDbfs,
                        row.stimulusSnrDb,
                        row.corrAecOff));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    VoIP RTL    : Acoustic = %.1f ms | HW TS = %s",
                        row.voipOffAcousticRtlMs,
                        formatMs(row.voipOffTimestampRtlMs)));

                // Check MMAP fallback on Native VoIP path (Android 18+ supports MMAP + sessionId AEC)
                if (isNative && Build.VERSION.SDK_INT >= 38
                        && mmapSupported
                        && (!row.voipOutMmap || !row.voipInMmap)) {
                    cb.onWarn();
                    Finding f = new Finding();
                    f.id = "VOIP-MMAP-FALLBACK";
                    f.severity = "WARNING";
                    f.title = "Native AAudio VOICE_COMMUNICATION dropped out of MMAP mode";
                    f.whatHappened = "The device supports AAudio MMAP, but opening a LowLatency stream with USAGE_VOICE_COMMUNICATION or INPUT_PRESET_VOICE_COMMUNICATION fell back to a non-MMAP legacy stream.";
                    f.androidContract = "On Android 18+, low-latency VoIP and voice agent streams using AAudio PerformanceMode::LowLatency should stay on the MMAP path when hardware DSP AEC is supported.";
                    f.whyItMatters = "Falling back from MMAP to legacy AudioFlinger mixer/capture threads adds 20-60 ms of extra round-trip latency during voice communication.";
                    f.howToFix = "Configure the Audio HAL and audio_policy_configuration.xml so USAGE_VOICE_COMMUNICATION output and INPUT_PRESET_VOICE_COMMUNICATION input profiles support MMAP (MmapPlaybackThread / MmapCaptureThread) with hardware DSP AEC bound by sessionId.";
                    cb.onFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Device MMAP=YES -> VoIP MMAP (Out=%s, In=%s).",
                                    row.apiLabel,
                                    row.voipOutMmap ? "YES" : "NO",
                                    row.voipInMmap ? "YES" : "NO"));
                }

                // =====================================================================
                // Phase 4: Cancelled VoIP Echo & ERLE (VOICE_COMMUNICATION, AEC ON)
                // =====================================================================
                cb.onPhaseStatus(row, 4, "RUNNING...");
                cb.onProgress(row.apiLabel
                        + " - Phase 4/5: Cancelled VoIP Echo & ERLE (AEC ON)...");
                cb.onTelemetry(
                        "  [Phase 4] Cancelled VoIP Echo & ERLE (VOICE_COMMUNICATION, AEC ON)");

                if (aec != null) {
                    try {
                        aec.setEnabled(true);
                    } catch (Exception ignored) {
                    }
                }
                player.restartIfDisconnected();

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
                double p4RmsSum = 0.0;
                int p4Count = 0;
                long p4StartMs = SystemClock.elapsedRealtime();
                long p4WarmupEnd = p4StartMs + 800;
                long end4 = p4StartMs + PHASE_SPEECH_MS;
                long nextShot4 = p4StartMs + SHOT_INTERVAL_MS;

                while (SystemClock.elapsedRealtime() < end4 && cb.isWorkerEnabled()) {
                    int read = isNative
                            ? TestVoiceCommAecActivity.nativeReadStream(
                                    nativeRec, ioBuf, 0, burst, 100_000_000L)
                            : javaRec.read(ioBuf, 0, burst, AudioRecord.READ_BLOCKING);
                    if (isNative && read < 0 && read != OBOE_ERROR_TIMEOUT) {
                        cb.onTelemetry("    WARN: nativeReadStream returned " + read + " in Phase 4");
                        break;
                    } else if (!isNative && read < 0) {
                        cb.onTelemetry("    WARN: AudioRecord.read returned " + read + " in Phase 4");
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
                        if (SystemClock.elapsedRealtime() >= p4WarmupEnd) {
                            p4RmsSum += computeBlockRms(ioBuf, read);
                            p4Count++;
                        }
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

                if (isNative) {
                    row.voipOnTimestampRtlMs =
                            TestVoiceCommAecActivity.nativeGetRoundTripTimestampLatencyMillis(
                                    player.getNativeHandle(), nativeRec);
                } else {
                    row.voipOnTimestampRtlMs = computeJavaTimestampLatencyMillis(
                            player, javaRec, javaFramesReadComm);
                }

                player.setPlaySpeech(false);
                player.setRmsTracker(null);
                playerEnv4.setRunning(false);
                recEnv4.setRunning(false);

                cb.onCaptureRecorded(
                        row.apiLabel, recAecOff, recAecOffLen, recAecOn, recAecOnLen);

                if (!cb.isWorkerEnabled()) return;

                double avgOnRms = (p4Count > 0) ? (p4RmsSum / p4Count) : MIN_RMS_VAL;
                row.aecOnDbfs = 20.0 * Math.log10(Math.max(avgOnRms, MIN_RMS_VAL));
                row.selfErleDb = row.aecOffDbfs - row.aecOnDbfs;
                row.distToFloorDb = Math.max(0.0, row.aecOnDbfs - row.noiseFloorDbfs);

                double silenceClampDb = Math.max(row.noiseFloorDbfs + 3.0, -52.0);
                row.corrAecOn = computeEnvelopeCorrelation(
                        playerEnv4.getSnapshotsCopy(),
                        recEnv4.getSnapshotsCopy(),
                        silenceClampDb);
                row.metricsPopulated = true;

                boolean erleOk = (row.selfErleDb >= THRESHOLD_ERLE_DB)
                        || (row.distToFloorDb <= THRESHOLD_DIST_TO_FLOOR_DB
                                && row.selfErleDb >= THRESHOLD_MIN_ERLE_AT_FLOOR_DB);
                boolean corrOnOk = (row.corrAecOn <= THRESHOLD_CORR_AEC_ON_MAX);
                boolean p4Pass = (p4Count > 0 && recAecOnLen > 0) && erleOk && corrOnOk;

                cb.onPhaseStatus(
                        row,
                        4,
                        String.format(
                                Locale.US,
                                "%s (ERLE %.1fdB, C=%.2f, %.0fms)",
                                p4Pass ? "PASS" : "FAIL",
                                row.selfErleDb,
                                row.corrAecOn,
                                row.voipOnTimestampRtlMs));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    AEC ON Lvl  : %.1f dBFS (Self-ERLE = %.1f dB, min >= %.1f dB)",
                        row.aecOnDbfs,
                        row.selfErleDb,
                        THRESHOLD_ERLE_DB));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Residual    : Dist-to-Floor = %.1f dB | Corr = %.2f (<= %.2f)",
                        row.distToFloorDb,
                        row.corrAecOn,
                        THRESHOLD_CORR_AEC_ON_MAX));
                cb.onTelemetry(String.format(
                        Locale.US,
                        "    AEC ON RTL  : HW TS = %s -> %s",
                        formatMs(row.voipOnTimestampRtlMs),
                        p4Pass ? "PASS" : "FAIL"));

                if (p4Pass) {
                    cb.onPass();
                } else {
                    cb.onFail();
                    Finding f = new Finding();
                    f.id = "AEC-INSUFFICIENT-CANCELLATION";
                    f.severity = "FAIL";
                    f.title = "Active AEC failed ERLE (>= 15.0 dB) or residual envelope correlation (<= 0.50)";
                    f.whatHappened = "With AcousticEchoCanceler.setEnabled(true) on VOICE_COMMUNICATION, loudspeaker speech bled into the captured microphone signal.";
                    f.androidContract = "5-Phase AudioAEC requires self-referenced ERLE >= 15.0 dB (or Dist-to-Floor <= 6.0 dB with ERLE >= 8.0 dB) and Residual Envelope Correlation <= 0.50 on both Java and Native AAudio VoIP paths.";
                    f.whyItMatters = "Poor echo cancellation causes conversational voice agents to hear and transcribe their own TTS output (self-interruption) and causes echo in VoIP calls.";
                    f.howToFix = "1. Verify that the HAL feeds the speakerphone playback echo_reference into the hardware AEC module for both Legacy and MMAP (USAGE_VOICE_COMMUNICATION) output streams.\n  2. Check for sample-rate or DMA timestamp jumps between output and input streams that prevent the adaptive AEC filter from converging.";
                    cb.onFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: Self-ERLE = %.1f dB (AEC OFF %.1f -> AEC ON %.1f dBFS), Dist-to-Floor = %.1f dB, Corr = %.2f (blocks=%d).",
                                    row.apiLabel,
                                    row.selfErleDb,
                                    row.aecOffDbfs,
                                    row.aecOnDbfs,
                                    row.distToFloorDb,
                                    row.corrAecOn,
                                    p4Count));
                }

                // Check 100ms Voice Agent Round-Trip Latency target on Native AAudio
                double bestVoipRtlMs = (row.voipOnTimestampRtlMs > 0)
                        ? row.voipOnTimestampRtlMs
                        : row.voipOffAcousticRtlMs;
                if (isNative && bestVoipRtlMs > THRESHOLD_VOICE_AGENT_RTL_MS) {
                    cb.onWarn();
                    Finding f = new Finding();
                    f.id = "VOIP-RTL-EXCEEDS-100MS-TARGET";
                    f.severity = "WARNING";
                    f.title = "Native AAudio VoIP Round-Trip Latency exceeds 100.0 ms target";
                    f.whatHappened = "Measured full-duplex round-trip latency on the Native AAudio VOICE_COMMUNICATION path exceeded the 100.0 ms real-time budget.";
                    f.androidContract = "Low-latency interactive voice agents and VoIP communication target <= 100.0 ms round-trip audio latency.";
                    f.whyItMatters = "High VoIP path latency delays barge-in detection, degrades AEC filter convergence, and adds noticeable conversational lag.";
                    f.howToFix = "Ensure VOICE_COMMUNICATION streams use MMAP with small period/burst sizes (<= 5 ms) and avoid large DSP block-buffering queues in the HAL.";
                    cb.onFinding(
                            f,
                            row.apiLabel,
                            String.format(
                                    Locale.US,
                                    "%s: VoIP Acoustic RTL = %.1f ms, VoIP HW Timestamp RTL = %s.",
                                    row.apiLabel,
                                    row.voipOffAcousticRtlMs,
                                    formatMs(row.voipOnTimestampRtlMs)));
                }
            }

            // =========================================================================
            // Phase 5: Multi-Turn Voice Agent Standby (>3.5s Pause + Resume)
            // =========================================================================
            if (runStandby && cb.isWorkerEnabled()) {
                cb.onPhaseStatus(row, 5, "RUNNING...");
                cb.onProgress(row.apiLabel + " - Phase 5/5: >3.5s Turn Pause & Resume...");
                cb.onTelemetry("  [Phase 5] Multi-Turn Standby (>3.5s Pause -> Resume)");

                if (aec != null) {
                    try {
                        aec.setEnabled(true);
                    } catch (Exception ignored) {
                    }
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

                if (isNative) {
                    row.standbyPreTsPos =
                            TestVoiceCommAecActivity.nativeGetTimestampPosition(nativeRec);
                    TestVoiceCommAecActivity.nativeStopStream(nativeRec);
                    player.pauseForStandby();
                } else {
                    AudioTimestamp ts = new AudioTimestamp();
                    if (javaRec.getTimestamp(ts, AudioTimestamp.TIMEBASE_MONOTONIC)
                            == AudioRecord.SUCCESS) {
                        row.standbyPreTsPos = ts.framePosition;
                    }
                    javaRec.stop();
                    player.pauseForStandby();
                }

                cb.onTelemetry(String.format(
                        Locale.US,
                        "    Sleeping %d ms to trigger HAL idle standby (preTs=%d)...",
                        STANDBY_PAUSE_MS,
                        row.standbyPreTsPos));
                if (!sleepInterruptible(STANDBY_PAUSE_MS, cb)) return;

                long resumeStartMs = SystemClock.elapsedRealtime();
                int startInRes = 0;
                int startOutRes = player.resumeAfterStandby();
                if (isNative) {
                    startInRes = TestVoiceCommAecActivity.nativeStartStream(nativeRec);
                } else {
                    javaRec.startRecording();
                }
                player.playSpeechFromStart();

                long loopStartMs = SystemClock.elapsedRealtime();
                long firstReadMs = -1;
                boolean nonZeroVerified = false;
                boolean aecEnabledAfterWake =
                        !aecSupported || (aec != null && aec.getEnabled());
                double postRmsSum = 0.0;
                int postCount = 0;
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
                        long elapsed = SystemClock.elapsedRealtime() - resumeStartMs;
                        if (firstReadMs < 0) {
                            firstReadMs = elapsed;
                            aecEnabledAfterWake =
                                    !aecSupported || (aec != null && aec.getEnabled());
                        }
                        double blockRms = computeBlockRms(ioBuf, read);
                        if (!nonZeroVerified && hasNonZeroSample(ioBuf, read)) {
                            nonZeroVerified = true;
                        }
                        postRmsSum += blockRms;
                        postCount++;
                    }
                }

                if (isNative) {
                    row.standbyPostTsPos =
                            TestVoiceCommAecActivity.nativeGetTimestampPosition(nativeRec);
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
                        && (!aecSupported || (aec != null && aec.getEnabled()));

                boolean tsValid;
                if (isNative) {
                    boolean expectNativeInputTs = row.voipInMmap
                            || row.standbyPreTsPos > 0
                            || row.standbyPostTsPos > 0;
                    tsValid = !expectNativeInputTs
                            || (row.standbyPostTsPos > 0
                                    && (row.standbyPreTsPos <= 0
                                            || row.standbyPostTsPos > row.standbyPreTsPos));
                } else {
                    tsValid = (row.standbyPostTsPos > 0);
                }
                String tsDesc = (row.standbyPostTsPos > 0)
                        ? (isNative ? "ts monotonic" : "ts active")
                        : "ts N/A";
                row.standbyPassed = (startInRes == 0 && startOutRes == 0)
                        && (postCount > 0 && firstReadMs >= 0 && firstReadMs <= 600)
                        && nonZeroVerified
                        && tsValid
                        && row.standbyAecStillEnabled;

                cb.onPhaseStatus(
                        row,
                        5,
                        row.standbyPassed
                                ? String.format(
                                        Locale.US,
                                        "PASS (%dms, %s, AEC)",
                                        firstReadMs,
                                        tsDesc)
                                : String.format(
                                        Locale.US,
                                        "FAIL (%dms, live=%b, ts %d->%d)",
                                        firstReadMs,
                                        nonZeroVerified,
                                        row.standbyPreTsPos,
                                        row.standbyPostTsPos));
                cb.onTelemetry(String.format(
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
                    cb.onPass();
                } else {
                    cb.onFail();
                    Finding f = new Finding();
                    f.id = "VOIP-STANDBY-RESUME-FAILURE";
                    f.severity = "FAIL";
                    f.title = "VoIP stream failed to resume cleanly after >3.5s multi-turn standby pause";
                    f.whatHappened = "After stopping the VOICE_COMMUNICATION stream for 3.5 seconds (allowing the HAL to enter idle standby) and restarting it, capture either stalled, returned dead zero samples, returned invalid timestamps, or lost its AcousticEchoCanceler state.";
                    f.androidContract = "AAudio.h & StreamDescriptor.aidl: Full-duplex VoIP streams paused for >3 seconds between conversational turns must resume delivering frames immediately on start() with valid timestamps and preserved audio effects.";
                    f.whyItMatters = "Conversational AI voice agents pause audio streams between user turns; standby resume bugs cause frozen microphone capture or broken AEC on subsequent turns.";
                    f.howToFix = "1. Ensure the Audio HAL starts the capture/playback PCM DMA driver when transitioning from IDLE to ACTIVE ('burst') after standby exit.\n  2. Preserve cumulative frame counters across MMAP FIFO recreation so getTimestamp().position does not jump backward after standby.";
                    cb.onFinding(
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
        } catch (Exception e) {
            Log.e(TAG, "Unexpected exception in runSingleApiPath (" + row.apiLabel + ")", e);
            cb.onTelemetry("    EXCEPTION in " + row.apiLabel + ": " + e.getMessage());
            if ("RUNNING...".equals(row.phase1Status)) cb.onPhaseStatus(row, 1, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase2Status)) cb.onPhaseStatus(row, 2, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase3Status)) cb.onPhaseStatus(row, 3, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase4Status)) cb.onPhaseStatus(row, 4, "FAIL (exception)");
            if ("RUNNING...".equals(row.phase5Status)) cb.onPhaseStatus(row, 5, "FAIL (exception)");
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
        cb.onTelemetry(String.format(
                Locale.US,
                "  VoIP Self-ERLE: Nat=%.1f dB | Java=%.1f dB\n",
                nat.selfErleDb,
                jav.selfErleDb));
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
        private AudioTrack mAudioTrack;
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
                if (mAudioTrack != null) {
                    try {
                        mAudioTrack.pause();
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
                if (mAudioTrack != null) {
                    try {
                        mAudioTrack.play();
                    } catch (Exception ignored) {
                    }
                }
                mPausedJava = false;
                return 0;
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
                mAudioTrack = builder.build();
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

            if (mIsNative) {
                int lastOffset = 0;
                synchronized (mNativeLock) {
                    if (mNativeHandle != 0) {
                        lastOffset = TestVoiceCommAecActivity.nativeGetPlaybackOffset(mNativeHandle);
                    }
                }
                while (mPlaying) {
                    if (mResetCursor) {
                        lastOffset = 0;
                        mResetCursor = false;
                    }
                    int currOffset = 0;
                    synchronized (mNativeLock) {
                        if (mNativeHandle == 0) break;
                        currOffset = TestVoiceCommAecActivity.nativeGetPlaybackOffset(mNativeHandle);
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
            } else if (mAudioTrack != null) {
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
                    int written = mAudioTrack.write(
                            writeBuf, 0, burst, AudioTrack.WRITE_BLOCKING);
                    if (written < 0) break;
                    mFramesWritten.addAndGet(written);
                }
            }
        }

        void stop() {
            mPlaying = false;
            mPausedJava = false;
            if (mThread != null) {
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
                        TestVoiceCommAecActivity.nativeStopStream(mNativeHandle);
                        TestVoiceCommAecActivity.nativeCloseStream(mNativeHandle);
                        mNativeHandle = 0;
                    }
                }
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
}
