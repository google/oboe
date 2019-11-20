/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef SAMPLES_ONESHOTPLAYER_H
#define SAMPLES_ONESHOTPLAYER_H

#include <oboe/Oboe.h>

#include <player/OneShotSampleBuffer.h>

using namespace oboe;
using namespace wavlib;

class OneShotPlayer : public AudioStreamCallback  {
public:
    OneShotPlayer(int numSampleBuffers, int channelCount, int sampleRate);

    // Inherited from oboe::AudioStreamCallback
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData,
            int32_t numFrames) override;
    void onErrorAfterClose(AudioStream *oboeStream, Result error) override;

    // Wave Sample Loading...
    // void loadSampleDataFromFile(const char* filePath, int index);
    void loadSampleDataFromAsset(byte* dataBytes, int dataLen, int index);

    void trigger(int index);

private:
    // Oboe Audio Stream
    AudioStream *mAudioStream { nullptr };
    bool openStream(int channelCount, int sampleRate);

    // Sample Data
    int mNumSampleBuffers;
    OneShotSampleBuffer* mSampleBuffers;
};

#endif //SAMPLES_ONESHOTPLAYER_H
