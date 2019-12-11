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

#ifndef _PLAYER_SIMIPLEMULTIPLAYER_H_
#define _PLAYER_SIMIPLEMULTIPLAYER_H_

#include <oboe/Oboe.h>

#include <player/OneShotSampleBuffer.h>

using namespace oboe;
using namespace wavlib;

/**
 * A simple streaming player for multiple SampleBuffers.
 */
class SimpleMultiPlayer : public AudioStreamCallback  {
public:
    SimpleMultiPlayer();

    // Inherited from oboe::AudioStreamCallback
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData,
            int32_t numFrames) override;
    void onErrorAfterClose(AudioStream *oboeStream, Result error) override;

    void setupAudioStream(int numSampleBuffers, int channelCount, int sampleRate);
    void teardownAudioStream();

    // Wave Sample Loading...
    void loadSampleDataFromAsset(byte* dataBytes, int dataLen, int index);
    void unloadSampleData();

    void triggerDown(int index);
    void triggerUp(int index);

private:
    // Oboe Audio Stream
    AudioStream *mAudioStream { nullptr };
    bool openStream(int channelCount, int sampleRate);

    // Sample Data
    int mNumSampleBuffers;
    OneShotSampleBuffer* mSampleBuffers;
};

#endif //_PLAYER_SIMIPLEMULTIPLAYER_H_
