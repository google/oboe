/**
 * Copyright 2017 The Android Open Source Project
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

#include "LatencyTuningCallback.h"

oboe::DataCallbackResult LatencyTuningCallback::onAudioReady(
     oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    if (bufferTuneEnabled && mLatencyTuner && oboeStream->getAudioApi() == oboe::AudioApi::AAudio) {
        mLatencyTuner->tune();
    }
    auto underrunCountResult = oboeStream->getXRunCount();
    int bufferSize = oboeStream->getBufferSizeInFrames();
    /**
     * The following output can be seen by running a systrace. Tracing is preferable to logging
    * inside the callback since tracing does not block.
    *
    * See https://developer.android.com/studio/profile/systrace-commandline.html
    */
    if (Trace::isEnabled()) Trace::beginSection("numFrames %d, Underruns %d, buffer size %d",
        numFrames, underrunCountResult.value(), bufferSize);
    auto result = DefaultAudioStreamCallback::onAudioReady(oboeStream, audioData, numFrames);
    if (latencyDetectionEnabled) calculateCurrentOutputLatencyMillis(oboeStream);
    if (Trace::isEnabled()) Trace::endSection();
    return result;
}

oboe::Result LatencyTuningCallback::calculateCurrentOutputLatencyMillis(oboe::AudioStream *stream) {
    // Get the time that a known audio frame was presented for playing
    auto result = stream->getTimestamp(CLOCK_MONOTONIC);
    if (result == oboe::Result::OK) {
        oboe::FrameTimestamp playedFrame = result.value();
        // Get the write index for the next audio frame
        int64_t writeIndex = stream->getFramesWritten();
        // Calculate the number of frames between our known frame and the write index
        int64_t frameIndexDelta = writeIndex - playedFrame.position;
        // Calculate the time which the next frame will be presented
        int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) /  (stream->getSampleRate());
        int64_t nextFramePresentationTime = playedFrame.timestamp + frameTimeDelta;
        // Assume that the next frame will be written at the current time
        using namespace std::chrono;
        int64_t nextFrameWriteTime =
                duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
        // Calculate the latency
        *outputLatencyMillis = static_cast<double>(nextFramePresentationTime - nextFrameWriteTime)
                         / kNanosPerMillisecond;
    } else {
        LOGE("Error calculating latency: %s", oboe::convertToText(result.error()));
    }
    return result;
}

