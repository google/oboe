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

#ifndef AUDIOPROCESSOR_H_
#define AUDIOPROCESSOR_H_

#include <cassert>
#include <cstring>
#include <math.h>
#include <jni.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>

#define MAX_BLOCK_SIZE   2048

class AudioProcessorBase;

class AudioInputPort;

typedef int32_t AudioResult;
#define AUDIO_RESULT_SUCCESS      0
#define AUDIO_RESULT_FAIL      -100


class AudioPort {
public:
    AudioPort(AudioProcessorBase &mParent, int samplesPerFrame);

    int getSamplesPerFrame() const { return mSamplesPerFrame; }

protected:
    AudioProcessorBase &mParent;
    int mSamplesPerFrame;
};

class AudioFloatPort  : public AudioPort {
public:
    AudioFloatPort(AudioProcessorBase &mParent, int samplesPerFrame);

    virtual ~AudioFloatPort();

    virtual float *getFloatBuffer(int numFrames);

protected:
    float   *mFloatBuffer;
};

class AudioOutputPort : public AudioFloatPort {
public:
    AudioOutputPort(AudioProcessorBase &parent, int samplesPerFrame);

    virtual ~AudioOutputPort() = default;

    using AudioFloatPort::getFloatBuffer;

    AudioResult pullData(
            uint64_t framePosition,
            int numFrames);

    void connect(AudioInputPort *port);
    void disconnect(AudioInputPort *port);
};

class AudioInputPort : public AudioFloatPort {
public:
    AudioInputPort(AudioProcessorBase &parent, int mSamplesPerFrame);

    virtual ~AudioInputPort() = default;

    float *getFloatBuffer(int numFrames);

    AudioResult pullData(
            uint64_t framePosition,
            int numFrames);

    void connect(AudioOutputPort *port) {
        assert(getSamplesPerFrame() == port->getSamplesPerFrame());
        mConnected = port;
    }
    void disconnect(AudioOutputPort *port) {
        assert(mConnected == port);
        mConnected = NULL;
    }
    void disconnect() {
        mConnected = NULL;
    }

    void setValue(float value);

private:
    AudioOutputPort *mConnected = nullptr;
};


class IAudioProcessor {
public:
    virtual AudioResult onProcess(
            uint64_t framePosition,
            int numFrames) = 0;
};

class AudioProcessorBase : public IAudioProcessor {
public:
    virtual ~AudioProcessorBase() = default;

    virtual AudioResult onProcess(
            uint64_t framePosition,
            int numFrames) = 0;

    AudioResult pullData(
            uint64_t framePosition,
            int numFrames);

    virtual void start() {
        mLastFramePosition = 0;
    }

    virtual void stop() {}

private:
    uint64_t    mLastFramePosition = 0;
    AudioResult mPreviousResult = AUDIO_RESULT_SUCCESS;
};

#endif /* AUDIOPROCESSOR_H_ */
