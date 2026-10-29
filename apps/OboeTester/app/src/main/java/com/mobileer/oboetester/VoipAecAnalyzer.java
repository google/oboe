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

import android.content.Context;
import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.net.Uri;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * UI-independent DSP analyzer and result models for the 5-Phase {@code VOICE_COMMUNICATION}
 * Acoustic Echo Cancellation (AEC), Round-Trip Latency (RTL), and Multi-Turn Standby diagnostic.
 *
 * <p>Reference Speech Asset Provenance: {@code res/raw/voice_speech_48k.mp3} is resampled to
 * 48 kHz mono from AOSP {@code cts/apps/CtsVerifier/res/raw/speech.mp3}
 * (Copyright The Android Open Source Project, licensed under the Apache License, Version 2.0).
 */
final class VoipAecAnalyzer {

    static final int SAMPLE_RATE = 48000;
    static final float MAX_VAL = 32768.0f;

    // Envelope correlation floor (-60 dBFS prevents room-noise jitter from dominating correlation)
    static final float MIN_RMS_DB = -60.0f;
    static final float MIN_RMS_VAL = (float) Math.pow(10.0, MIN_RMS_DB / 20.0);

    // True 16-bit PCM level floor (-96 dBFS preserves quiet silence baselines down to 1 LSB)
    static final double LEVEL_MIN_RMS_DB = -96.0;
    static final double LEVEL_MIN_RMS_VAL = Math.pow(10.0, LEVEL_MIN_RMS_DB / 20.0);

    static final int AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION = 7;
    static final int STREAM_STATE_DISCONNECTED = 13;
    static final int OBOE_ERROR_TIMEOUT = -885; // oboe::Result::ErrorTimeout (AAUDIO_ERROR_TIMEOUT)

    static final int PHASE_SILENCE_WARMUP_MS = 400;
    static final int PHASE_SILENCE_DURATION_MS = 1500;
    static final int PHASE_SPEECH_MS = 5000;
    static final int SPEECH_LEVEL_WARMUP_MS = 800; // Symmetric [0.8s, 5.0s) window for P3 & P4
    static final int SHOT_INTERVAL_MS = 200;
    static final int STANDBY_PAUSE_MS = 3500;      // 3.5s (>3.0s AAudio MMAP standby threshold)
    static final int STANDBY_RESUME_MS = 1200;
    static final int STANDBY_POST_WARMUP_MS = 300;
    static final int PLAYER_RMS_WINDOW_SAMPLES = 960; // 20 ms at 48 kHz

    // Diagnostic thresholds (aligned with 5-Phase CTS Verifier AudioAEC & 100ms Voice Agent target)
    static final double THRESHOLD_AMBIENT_NOISE_CEILING_DBFS = -35.0;
    static final double THRESHOLD_STIMULUS_SNR_DB = 12.0;
    static final double THRESHOLD_CORR_AEC_OFF_MIN = 0.60;
    static final double THRESHOLD_CORR_AEC_ON_MAX = 0.50;
    static final double THRESHOLD_ERLE_DB = 15.0;
    static final double THRESHOLD_DIST_TO_FLOOR_DB = 6.0;
    static final double THRESHOLD_MIN_ERLE_AT_FLOOR_DB = 8.0;
    static final double THRESHOLD_VOICE_AGENT_RTL_MS = 100.0;
    static final double MIN_LIVE_SAMPLE_VARIANCE = 1e-12;

    private static final int MAX_DECODE_STEPS = 20000;
    private static final int MAX_DECODED_BYTES = SAMPLE_RATE * 4 * 20;

    private VoipAecAnalyzer() {}

    static final class ApiRunMetrics {
        final boolean isNative;
        final String apiLabel;
        String configSummary = "Pending...";

        String phase1Status = "PENDING";
        String phase2Status = "PENDING";
        String phase3Status = "PENDING";
        String phase4Status = "PENDING";
        String phase5Status = "PENDING";

        boolean voipOutMmap = false;
        boolean voipInMmap = false;
        boolean voipOutExclusive = false;
        boolean voipInExclusive = false;
        int outBurstFrames = 0;
        int outCapacityFrames = 0;
        int voipInBurstFrames = 0;
        int voipInCapacityFrames = 0;

        // Effect attachment & control diagnostics (captured pre-start)
        int sessionId = -1;
        boolean aecAvailable = false;
        boolean aecCreated = false;
        boolean aecDefaultEnabled = false;
        boolean aecHasControl = false;
        int aecSetEnabledRc = Integer.MIN_VALUE;
        boolean aecControlVerified = false;

        // Phase 1 & Phase 2: Silence baselines on VOICE_COMMUNICATION (AEC OFF vs AEC ON)
        double noiseFloorDbfs = LEVEL_MIN_RMS_DB;
        double aecOnSilenceDbfs = LEVEL_MIN_RMS_DB;
        boolean phase2Measured = false;

        // Phase 3 & Phase 4: Speech echo levels & self-referenced dB deltas on VOICE_COMMUNICATION
        double aecOffDbfs = LEVEL_MIN_RMS_DB;
        double aecOnDbfs = LEVEL_MIN_RMS_DB;
        boolean aecOnMeasured = false;
        double stimulusSnrDb = 0.0;    // aecOffDbfs - noiseFloorDbfs
        double selfErleDb = 0.0;       // aecOffDbfs - aecOnDbfs (over symmetric [0.8s, 5.0s) window)
        double distToFloorDb = 0.0;    // aecOnDbfs - max(noiseFloorDbfs, aecOnSilenceDbfs)

        double corrAecOff = 0.0;
        double corrAecOn = 0.0;

        double voipOffAcousticRtlMs = -1.0;
        double voipOffTimestampRtlMs = -1.0;
        double voipOnTimestampRtlMs = -1.0;

        // Phase 5: Multi-Turn Standby (3.5s Pause + Resume)
        boolean standbyExecuted = false;
        boolean standbyPassed = false;
        long standbyFirstFrameMs = -1;
        long standbyPreTsPos = -1;
        long standbyPostTsPos = -1;
        long standbyOutFramesAdvanced = 0;
        double standbyPostRmsDbfs = LEVEL_MIN_RMS_DB;
        boolean standbyAecStillEnabled = false;

        boolean metricsPopulated = false;

        ApiRunMetrics(boolean isNative, String apiLabel) {
            this.isNative = isNative;
            this.apiLabel = apiLabel;
        }
    }

    static final class Finding {
        String id;
        String severity; // "FAIL", "WARNING", or "INFO"
        String title;
        final List<String> affectedPaths = new ArrayList<>();
        String whatHappened;
        final List<String> measuredEvidence = new ArrayList<>();
        String androidContract;
        String whyItMatters;
        String howToFix;
    }

    static final class DecodedSpeech {
        final short[] pcm;
        final int sampleRate;

        DecodedSpeech(short[] pcm, int sampleRate) {
            this.pcm = pcm;
            this.sampleRate = sampleRate;
        }
    }

    static final class SignalWindowStats {
        final int sampleCount;
        final boolean hasNonZero;
        final double rms;
        final double dbfs;
        final double sampleVariance;

        SignalWindowStats(
                int sampleCount,
                boolean hasNonZero,
                double rms,
                double dbfs,
                double sampleVariance) {
            this.sampleCount = sampleCount;
            this.hasNonZero = hasNonZero;
            this.rms = rms;
            this.dbfs = dbfs;
            this.sampleVariance = sampleVariance;
        }
    }

    static final class RmsEnvelopeTracker {
        private final double[] mSnapshots;
        private final double[] mPreemphSnapshots;
        private int mShotIndex = 0;
        private double mRmsCurrent = MIN_RMS_VAL;
        private double mRmsPreemphCurrent = MIN_RMS_VAL;
        private double mEnergyAccumulator = 0.0;
        private double mPreemphEnergyAccumulator = 0.0;
        private double mPrevSample = 0.0;
        private int mEnergyCount = 0;
        private volatile boolean mRunning = false;

        RmsEnvelopeTracker(int maxShots) {
            mSnapshots = new double[maxShots];
            mPreemphSnapshots = new double[maxShots];
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
            int limit = Math.min(numSamples, samples.length);
            for (int i = 0; i < limit; i++) {
                double raw = samples[i] / MAX_VAL;
                double hp = raw - 0.82 * mPrevSample;
                mPrevSample = raw;
                mEnergyAccumulator += raw * raw;
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

    static double rmsToDbfs(double rms) {
        return 20.0 * Math.log10(Math.max(rms, LEVEL_MIN_RMS_VAL));
    }

    static boolean hasNonZeroSample(short[] buf, int count) {
        if (buf == null || count <= 0) return false;
        int limit = Math.min(count, buf.length);
        for (int i = 0; i < limit; i++) {
            if (buf[i] != 0) return true;
        }
        return false;
    }

    static double computeBlockRms(short[] buf, int count) {
        if (buf == null || count <= 0) return LEVEL_MIN_RMS_VAL;
        int limit = Math.min(count, buf.length);
        if (limit <= 0) return LEVEL_MIN_RMS_VAL;
        double sumSq = 0.0;
        for (int i = 0; i < limit; i++) {
            double v = buf[i] / MAX_VAL;
            sumSq += v * v;
        }
        return Math.max(Math.sqrt(sumSq / limit), LEVEL_MIN_RMS_VAL);
    }

    static SignalWindowStats computeWindowStats(short[] pcm, int startSample, int endSample) {
        if (pcm == null || pcm.length == 0) {
            return new SignalWindowStats(0, false, LEVEL_MIN_RMS_VAL, LEVEL_MIN_RMS_DB, 0.0);
        }
        int start = Math.max(0, Math.min(startSample, pcm.length));
        int end = Math.max(start, Math.min(endSample, pcm.length));
        int count = end - start;
        if (count <= 0) {
            return new SignalWindowStats(0, false, LEVEL_MIN_RMS_VAL, LEVEL_MIN_RMS_DB, 0.0);
        }
        boolean nonZero = false;
        double sum = 0.0;
        double sumSq = 0.0;
        for (int i = start; i < end; i++) {
            short s = pcm[i];
            if (s != 0) nonZero = true;
            double v = s / MAX_VAL;
            sum += v;
            sumSq += v * v;
        }
        double meanSq = sumSq / count;
        double mean = sum / count;
        double variance = Math.max(0.0, meanSq - (mean * mean));
        double rms = Math.max(Math.sqrt(meanSq), LEVEL_MIN_RMS_VAL);
        return new SignalWindowStats(count, nonZero, rms, rmsToDbfs(rms), variance);
    }

    static double computeEnvelopeCorrelation(
            double[] playerRms, double[] recRms, double recFloorDb) {
        if (playerRms == null || recRms == null) return 0.0;
        int len = Math.min(playerRms.length, recRms.length);
        int last = len - 1;
        int first = Math.max(0, last - 15);
        int n = last - first + 1;
        if (n <= 3) return 0.0;

        double[] a = new double[n];
        double[] b = new double[n];
        double sumA = 0.0;
        double sumB = 0.0;
        for (int i = 0; i < n; i++) {
            a[i] = Math.max(
                    20.0 * Math.log10(Math.max(playerRms[first + i], MIN_RMS_VAL)), MIN_RMS_DB);
            b[i] = Math.max(
                    20.0 * Math.log10(Math.max(recRms[first + i], MIN_RMS_VAL)), recFloorDb);
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
        double effVarB = (recFloorDb > MIN_RMS_DB) ? Math.max(varB, 4.0) : varB;
        double denom = Math.sqrt(varA * effVarB);
        if (denom <= 1e-7) return 0.0;

        double maxCorr = 0.0;
        int maxLag = Math.min(3, n - 3);
        for (int lag = 0; lag <= maxLag; lag++) {
            int overlap = n - lag;
            if (overlap <= 2) break;
            double cross = 0.0;
            for (int j = 0; j < overlap; j++) {
                cross += (a[j] - meanA) * (b[lag + j] - meanB);
            }
            double corr = cross / (overlap * denom);
            if (corr > maxCorr) {
                maxCorr = corr;
            }
        }
        return maxCorr;
    }

    static double computeAcousticLatencyMs(
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

        int coarseLagStep = 4;
        int coarseIntStride = 16;
        double maxCorr = -1.0;
        int bestLag = 0;

        for (int lag = 0; lag < maxLagSamples; lag += coarseLagStep) {
            double dot = 0.0;
            double normRef = 0.0;
            double normRec = 0.0;
            for (int i = 0; i < windowRecSamples; i += coarseIntStride) {
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

        int fineStart = Math.max(0, bestLag - 16);
        int fineEnd = Math.min(maxLagSamples, bestLag + 16);
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
        if (maxCorr < 0.08) return -1.0;
        return (bestLag * 1000.0) / recRate;
    }

    /**
     * Decodes {@code res/raw/voice_speech_48k.mp3} (resampled to 48 kHz mono from AOSP
     * {@code cts/apps/CtsVerifier/res/raw/speech.mp3}, Apache License 2.0) into 16-bit mono PCM.
     */
    static DecodedSpeech decodeSpeechResource(Context context, int resId) {
        Uri uri = Uri.parse("android.resource://" + context.getPackageName() + "/" + resId);
        MediaExtractor extractor = new MediaExtractor();
        MediaCodec codec = null;
        int sampleRate = SAMPLE_RATE;
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
            if (trackIndex >= 0 && format != null) {
                extractor.selectTrack(trackIndex);
                MediaCodecList mclist = new MediaCodecList(MediaCodecList.ALL_CODECS);
                String decoderName = mclist.findDecoderForFormat(format);
                if (decoderName != null) {
                    codec = MediaCodec.createByCodecName(decoderName);
                    codec.configure(format, null, null, 0);
                    codec.start();

                    MediaFormat outputFormat = format;
                    ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
                    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                    boolean sawInputEOS = false;
                    boolean sawOutputEOS = false;
                    int steps = 0;

                    while (!sawOutputEOS && steps < MAX_DECODE_STEPS
                            && byteStream.size() < MAX_DECODED_BYTES) {
                        steps++;
                        if (!sawInputEOS) {
                            int inIdx = codec.dequeueInputBuffer(5000);
                            if (inIdx >= 0) {
                                ByteBuffer inBuf = codec.getInputBuffer(inIdx);
                                int sampleSize = (inBuf != null)
                                        ? extractor.readSampleData(inBuf, 0)
                                        : -1;
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
                        if (outIdx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                            outputFormat = codec.getOutputFormat();
                        } else if (outIdx >= 0) {
                            ByteBuffer outBuf = codec.getOutputBuffer(outIdx);
                            if (outBuf != null && info.size > 0) {
                                outBuf.position(info.offset);
                                outBuf.limit(info.offset + info.size);
                                byte[] chunk = new byte[info.size];
                                outBuf.get(chunk);
                                outBuf.clear();
                                byteStream.write(chunk, 0, chunk.length);
                            }
                            codec.releaseOutputBuffer(outIdx, false);
                            if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                                sawOutputEOS = true;
                            }
                        }
                    }

                    if (outputFormat != null
                            && outputFormat.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
                        sampleRate = outputFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                    } else if (format.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
                        sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                    }
                    int channels = (outputFormat != null
                            && outputFormat.containsKey(MediaFormat.KEY_CHANNEL_COUNT))
                            ? Math.max(1, outputFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT))
                            : 1;
                    int pcmEncoding = (outputFormat != null
                            && outputFormat.containsKey(MediaFormat.KEY_PCM_ENCODING))
                            ? outputFormat.getInteger(MediaFormat.KEY_PCM_ENCODING)
                            : AudioFormat.ENCODING_PCM_16BIT;

                    byte[] pcmBytes = byteStream.toByteArray();
                    short[] monoShorts = convertDecodedBytesToMonoShorts(
                            pcmBytes, channels, pcmEncoding);
                    if (monoShorts.length >= SAMPLE_RATE) {
                        return new DecodedSpeech(monoShorts, sampleRate);
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TestVoiceCommAecActivity.TAG, "Failed to decode voice_speech_48k resource", e);
        } finally {
            if (codec != null) {
                try {
                    codec.stop();
                } catch (Exception ignored) {
                }
                try {
                    codec.release();
                } catch (Exception ignored) {
                }
            }
            extractor.release();
        }
        return new DecodedSpeech(generateSyntheticSpeechFallback(SAMPLE_RATE, 6), SAMPLE_RATE);
    }

    private static short[] convertDecodedBytesToMonoShorts(
            byte[] pcmBytes, int channels, int pcmEncoding) {
        int ch = Math.max(1, channels);
        if (pcmEncoding == AudioFormat.ENCODING_PCM_FLOAT) {
            FloatBuffer fb = ByteBuffer.wrap(pcmBytes)
                    .order(ByteOrder.LITTLE_ENDIAN)
                    .asFloatBuffer();
            int frames = fb.remaining() / ch;
            short[] mono = new short[frames];
            for (int f = 0; f < frames; f++) {
                float sum = 0.0f;
                for (int c = 0; c < ch; c++) {
                    sum += fb.get();
                }
                float avg = (sum / ch) * 32767.0f;
                mono[f] = (short) Math.max(-32768.0f, Math.min(32767.0f, avg));
            }
            return mono;
        }
        ShortBuffer sb = ByteBuffer.wrap(pcmBytes)
                .order(ByteOrder.LITTLE_ENDIAN)
                .asShortBuffer();
        int totalShorts = sb.remaining();
        if (ch == 1) {
            short[] mono = new short[totalShorts];
            sb.get(mono);
            return mono;
        }
        int frames = totalShorts / ch;
        short[] mono = new short[frames];
        for (int f = 0; f < frames; f++) {
            int sum = 0;
            for (int c = 0; c < ch; c++) {
                sum += sb.get();
            }
            mono[f] = (short) (sum / ch);
        }
        return mono;
    }

    static short[] generateSyntheticSpeechFallback(int sampleRate, int seconds) {
        int total = sampleRate * seconds;
        short[] out = new short[total];
        long seed = 0x5A17C9E3L;
        for (int i = 0; i < total; i++) {
            double t = (double) i / sampleRate;
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
}
