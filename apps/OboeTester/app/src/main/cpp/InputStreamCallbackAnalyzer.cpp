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

#include "common/OboeDebug.h"
#include "InputStreamCallbackAnalyzer.h"

oboe::DataCallbackResult InputStreamCallbackAnalyzer::onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int numFrames) {
    int32_t channelCount = audioStream->getChannelCount();

    if (audioStream->getFormat() == oboe::AudioFormat::I16) {
        int16_t *shortData = (int16_t *) audioData;
        if (mRecording != nullptr) {
            mRecording->write(shortData, numFrames);
        }
        int16_t *frameData = shortData;
        for (int iFrame = 0; iFrame < numFrames; iFrame++) {
            for (int iChannel = 0; iChannel < channelCount; iChannel++) {
                float sample = frameData[iChannel] / 32768.0f;
                mPeakDetectors[iChannel].process(sample);
            }
            frameData += channelCount;
        }
    } else if (audioStream->getFormat() == oboe::AudioFormat::Float) {
        float *floatData = (float *) audioData;
        if (mRecording != nullptr) {
            mRecording->write(floatData, numFrames);
        }
        float *frameData = floatData;
        for (int iFrame = 0; iFrame < numFrames; iFrame++) {
            for (int iChannel = 0; iChannel < channelCount; iChannel++) {
                float sample = frameData[iChannel];
                mPeakDetectors[iChannel].process(sample);
            }
            frameData += channelCount;
        }
    }

    audioStream->waitForAvailableFrames(mMinimumFramesBeforeRead, oboe::kNanosPerSecond);

    return oboe::DataCallbackResult::Continue;
}
