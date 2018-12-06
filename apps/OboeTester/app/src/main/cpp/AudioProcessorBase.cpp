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
/*
 * AudioProcessor.h
 *
 * Processing node in an audio graph.
 */
#include <sys/types.h>
#define MODULE_NAME "NatRingMole"
#include "common/OboeDebug.h"
#include "oboe/Oboe.h"
#include "AudioProcessorBase.h"

AudioPort::AudioPort(AudioProcessorBase &parent, int samplesPerFrame)
        : mParent(parent)
        , mSamplesPerFrame(samplesPerFrame) {
}

AudioFloatPort::AudioFloatPort(AudioProcessorBase &parent, int samplesPerFrame)
        : AudioPort(parent, samplesPerFrame), mFloatBuffer(NULL) {
    int numFloats = MAX_BLOCK_SIZE * mSamplesPerFrame;
    mFloatBuffer = new float[numFloats]{};
}

AudioFloatPort::~AudioFloatPort() {
    delete[] mFloatBuffer;
}

float *AudioFloatPort::getFloatBuffer(int numFrames) {
    assert(numFrames <= MAX_BLOCK_SIZE);
    return mFloatBuffer;
}

AudioOutputPort::AudioOutputPort(AudioProcessorBase &parent, int samplesPerFrame)
            : AudioFloatPort(parent, samplesPerFrame)
{
    LOGD("AudioOutputPort(%d)", samplesPerFrame);
}

AudioResult AudioOutputPort::pullData(
        uint64_t framePosition,
        int numFrames) {
    return mParent.pullData(framePosition, numFrames);
}

void AudioOutputPort::connect(AudioInputPort *port) {
    port->connect(this);
}
void AudioOutputPort::disconnect(AudioInputPort *port) {
    port->disconnect(this);
}

AudioInputPort::AudioInputPort(AudioProcessorBase &parent, int samplesPerFrame)
        : AudioFloatPort(parent, samplesPerFrame)
{
}

AudioResult AudioInputPort::pullData(
        uint64_t framePosition,
        int numFrames) {
    return (mConnected == nullptr)
        ? AUDIO_RESULT_SUCCESS
        : mConnected->pullData(framePosition, numFrames);
}

float *AudioInputPort::getFloatBuffer(int numFrames) {
    if (mConnected == NULL) {
        return AudioFloatPort::getFloatBuffer(numFrames);
    } else {
        return mConnected->getFloatBuffer(numFrames);
    }
}

void AudioInputPort::setValue(float value) {
    int numFloats = MAX_BLOCK_SIZE * mSamplesPerFrame;
    for (int i = 0; i < numFloats; i++) {
        mFloatBuffer[i] = value;
    }
}

/*
 * AudioProcessorBase
 */

AudioResult AudioProcessorBase::pullData(
        uint64_t framePosition,
        int numFrames) {
    if (framePosition > mLastFramePosition) {
        mLastFramePosition = framePosition;
        mPreviousResult = onProcess(framePosition, numFrames);
    }
    return mPreviousResult;
}
