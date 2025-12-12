/*
 * Copyright 2015 The Android Open Source Project
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

import java.util.ArrayList;
import android.util.Log; 
/**
 * Analyze a recording and extract edges for latency analysis.
 */
public class TapLatencyAnalyser {
    public static final int TYPE_TAP = 0;
    public static final int TYPE_TONE =1;
    float[] mHighPassBuffer;

    private float mDroop = 0.995f;
    private static final float EDGE_THRESHOLD = 0.01f;
    private static final float LOW_FRACTION = 0.5f;

    public static class TapLatencyEvent {
        public int type;
        public int sampleIndex;
        public TapLatencyEvent(int type, int sampleIndex) {
            this.type = type;
            this.sampleIndex = sampleIndex;
        }
    }

    /**
     * Analyzes the provided audio data to find audio event "edges"
     *
     * @param buffer Audio samples to analyze.
     * @param offset Offset within the provide buffer to start analysis.
     * @param numSamples Number of samples to analyze.
     * @return An array of TapLatencyEvent objects.
     */
    public TapLatencyEvent[] analyze(float[] buffer, int offset, int numSamples) {
        // Use high pass filter to remove rumble from air conditioners.
        mHighPassBuffer = new float[numSamples];
        highPassFilter(buffer, offset, numSamples, mHighPassBuffer);
        // Apply envelope follower.
        float[] peakBuffer = new float[numSamples];
        fillPeakBuffer(mHighPassBuffer, 0, numSamples, peakBuffer);
        // Look for two attacks.
        return scanForEdges(peakBuffer, numSamples);
    }

    /**
     * @return The filtered samples on which the analysis was performed.
     *   High-pass filtered to emphasize high-frequency events such as edges.
     */
    public float[] getFilteredBuffer() {
        return mHighPassBuffer;
    }

    // Based on https://en.wikipedia.org/wiki/High-pass_filter
    private void highPassFilter(
            float[] buffer, int offset, int numSamples, float[] highPassBuffer) {
        float xn1 = 0.0f;
        float yn1 = 0.0f;
        // This alpha value was chosen empirically to attenuate low-frequency rumble from air
        // conditioners, etc. while still passing the high-frequency edges of the tap and blip.
        // A higher value will attenuate lower frequencies more strongly.
        // This is 400Hz at a 48kHz sample rate.
        // fc = (1 - alpha) * fs / (2 * pi * alpha)
        final float alpha = 0.95f;
        for (int i = 0; i < numSamples; i++) {
            float xn = buffer[i + offset];
            float yn = alpha * (yn1 + xn - xn1);
            highPassBuffer[i] = yn;
            xn1 = xn;
            yn1 = yn;
        }
    }

    private TapLatencyEvent[] scanForEdges(float[] peakBuffer, int numSamples) {
        ArrayList<TapLatencyEvent> events = new ArrayList<TapLatencyEvent>();
        float slow = 0.0f;
        float fast = 0.0f;
        final float slowCoefficient = 0.01f;
        final float fastCoefficient = 0.10f;
    	final float EDGE_THRESHOLD = 0.01f;     // trigger threshold for tap
        final float REARM_FRACTION = 0.3f;      // fraction of last peak to rearm
        final int MIN_TONE_DELAY = 2000;       //  samples between TAP and TONE (~45 ms at 44.1kHz)
        float lastPeak = 0.0f;
        boolean armed = true;
        boolean tapDetected = false;
        boolean toneDetected = false;
        int lastTapIndex = -1;

        int sampleIndex = 0;

        for (float level : peakBuffer) {
		    slow = slow + (level - slow) * slowCoefficient; // low pass filter
		    fast = fast + (level - fast) * fastCoefficient; // low pass filter
		    if (armed && !tapDetected && (fast > EDGE_THRESHOLD) && (fast > 2.0f * slow)) {
			    Log.i("TTL", "TTL Event of TYPE_TAP detected at " + sampleIndex + " fast=" + fast);
			    events.add(new TapLatencyEvent(TYPE_TAP, sampleIndex));
			    armed = false;
			    tapDetected = true;
			    lastPeak = fast;
               	lastTapIndex = sampleIndex;
            }

	    	if (tapDetected && !toneDetected && !armed && (sampleIndex - lastTapIndex) > MIN_TONE_DELAY) {
                if ((fast > EDGE_THRESHOLD) && (fast > 1.5f * slow)) {
                    Log.i("TTL", "TTL Event of TYPE_TONE detected at " + sampleIndex + " fast=" + fast);
                    events.add(new TapLatencyEvent(TYPE_TONE, sampleIndex));
                    toneDetected = true;
                }
            }

            // Rearm only when signal has fallen enough
            if (!armed && (fast < lastPeak * REARM_FRACTION)) {
                Log.i("TTL", "Signal has settled, rearming now (fast=" + fast + ")");
			    armed = true;
            }
            sampleIndex++;
        }
        return events.toArray(new TapLatencyEvent[0]);
    }

    /**
     * Envelope follower that rides along the peaks of the waveforms and then decays exponentially.
     *
     * @param buffer Input buffer of samples.
     * @param offset Offset into the input buffer.
     * @param numSamples Number of samples to process.
     * @param peakBuffer Output buffer of peak-following samples.
     */
    private void fillPeakBuffer(float[] buffer, int offset, int numSamples, float[] peakBuffer) {
        float previous = 0.0f;
        for (int i = 0; i < numSamples; i++) {
            float input = Math.abs(buffer[i + offset]);
            float output = previous * mDroop;
            if (input > output) {
                output = input;
            }
            previous = output;
            peakBuffer[i] = output;
        }
    }
}
