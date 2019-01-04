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
 * AudioProcessorBase.h
 *
 * Audio processing node and ports that can be used in a simple data flow graph.
 */

#ifndef FLOWGRAPH_AUDIO_PROCESSOR_BASE_H
#define FLOWGRAPH_AUDIO_PROCESSOR_BASE_H

#include <cassert>
#include <cstring>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Set this to 1 if using it inside the Android framework.
#define FLOWGRAPH_ANDROID_INTERNAL 0

namespace flowgraph {

// Default block size that can be overridden when the AudioFloatBlockPort is created.
// If it is too small then we will have too much overhead from switching between nodes.
// If it is too high then we will thrash the caches.
constexpr int kDefaultBlockSize = 8; // arbitrary

class AudioFloatInputPort;

/***************************************************************************/
class AudioProcessorBase {
public:
    virtual ~AudioProcessorBase() = default;

    /**
     * Perform custom function.
     *
     * @param framePosition index of first frame to be processed
     * @param numFrames maximum number of frames requested for processing
     * @return number of frames actually processed
     */
    virtual int32_t onProcess(int64_t framePosition, int32_t numFrames) = 0;

    /**
     * If the framePosition is at or after the last frame position then call onProcess().
     * This prevents infinite recursion in case of cyclic graphs.
     * It also prevents nodes upstream from a branch from being executed twice.
     *
     * @param framePosition
     * @param numFrames
     * @return number of frames valid
     */
    int32_t pullData(int64_t framePosition, int32_t numFrames);

    virtual void start() {
        mLastFramePosition = 0;
    }

    virtual void stop() {}

protected:
    int64_t  mLastFramePosition = -1; // Start at -1 so that the first pull works.

private:
    int32_t  mFramesValid = 0; // num valid frames in the block
};

/***************************************************************************/
/**
  * This is a connector that allows data to flow between modules.
  */
class AudioPort {
public:
    AudioPort(AudioProcessorBase &parent, int32_t samplesPerFrame)
            : mParent(parent)
            , mSamplesPerFrame(samplesPerFrame) {
    }

    // Ports are often declared public. So let's make them non-copyable.
    AudioPort(const AudioPort&) = delete;
    AudioPort& operator=(const AudioPort&) = delete;

    int32_t getSamplesPerFrame() const {
        return mSamplesPerFrame;
    }

protected:
    AudioProcessorBase &mParent;

private:
    const int32_t    mSamplesPerFrame = 1;
};

/***************************************************************************/
/**
 * This port contains a float type buffer.
 * The size is framesPerBlock * samplesPerFrame).
 */
class AudioFloatBlockPort  : public AudioPort {
public:
    AudioFloatBlockPort(AudioProcessorBase &mParent,
                   int32_t samplesPerFrame,
                   int32_t framesPerBlock = kDefaultBlockSize
                );

    virtual ~AudioFloatBlockPort();

    int32_t getFramesPerBlock() const {
        return mFramesPerBlock;
    }

protected:

    /**
     * @return buffer internal to the port or from a connected port
     */
    virtual float *getBlock() {
        return mSampleBlock;
    }


private:
    const int32_t    mFramesPerBlock = 1;
    float           *mSampleBlock = nullptr; // allocated in constructor
};

/***************************************************************************/
/**
  * The results of a module are stored in the buffer of the output ports.
  */
class AudioFloatOutputPort : public AudioFloatBlockPort {
public:
    AudioFloatOutputPort(AudioProcessorBase &parent, int32_t samplesPerFrame)
            : AudioFloatBlockPort(parent, samplesPerFrame) {
    }

    virtual ~AudioFloatOutputPort() = default;

    using AudioFloatBlockPort::getBlock;

    /**
     * Call the parent module's onProcess() method.
     * That may pull data from its inputs and recursively
     * process the entire graph.
     * @return number of frames actually pulled
     */
    int32_t pullData(int64_t framePosition, int32_t numFrames);

    /**
     * Connect to the input of another module.
     * An input port can only have one connection.
     * An output port can have multiple connections.
     * If you connect a second output port to an input port
     * then it overwrites the previous connection.
     *
     * This not thread safe. Do not modify the graph topology from another thread while running.
     * Also do not delete a module while it is connected to another port if the graph is running.
     */
    void connect(AudioFloatInputPort *port);

    /**
     * Disconnect from the input of another module.
     * This not thread safe.
     */
    void disconnect(AudioFloatInputPort *port);
};

/***************************************************************************/
class AudioFloatInputPort : public AudioFloatBlockPort {
public:
    AudioFloatInputPort(AudioProcessorBase &parent, int32_t samplesPerFrame)
            : AudioFloatBlockPort(parent, samplesPerFrame) {
    }

    virtual ~AudioFloatInputPort() = default;

    /**
     * If connected to an output port then this will return
     * that output ports buffers.
     * If not connected then it returns the input ports own buffer
     * which can be loaded using setValue().
     */
    float *getBlock() override;

    /**
     * Pull data from any output port that is connected.
     */
    int32_t pullData(int64_t framePosition, int32_t numFrames);

    /**
     * Write every value of the float buffer.
     * This value will be ignored if an output port is connected
     * to this port.
     */
    void setValue(float value) {
        int numFloats = kDefaultBlockSize * getSamplesPerFrame();
        float *buffer = getBlock();
        for (int i = 0; i < numFloats; i++) {
            *buffer++ = value;
        }
    }

    /**
     * Connect to the output of another module.
     * An input port can only have one connection.
     * An output port can have multiple connections.
     * This not thread safe.
     */
    void connect(AudioFloatOutputPort *port) {
        assert(getSamplesPerFrame() == port->getSamplesPerFrame());
        mConnected = port;
    }

    void disconnect(AudioFloatOutputPort *port) {
        assert(mConnected == port);
        (void) port;
        mConnected = nullptr;
    }

    void disconnect() {
        mConnected = nullptr;
    }

private:
    AudioFloatOutputPort *mConnected = nullptr;
};

/***************************************************************************/
class AudioSource : public AudioProcessorBase {
public:
    explicit AudioSource(int32_t channelCount)
            : output(*this, channelCount) {
    }

    virtual ~AudioSource() = default;

    AudioFloatOutputPort output;

    void setData(const void *data, int32_t numFrames) {
        mData = data;
        mSizeInFrames = numFrames;
        mFrameIndex = 0;
    }

protected:
    const void *mData = nullptr;
    int32_t     mSizeInFrames = 0; // number of frames in mData
    int32_t     mFrameIndex = 0; // index of next frame to be processed
};

/***************************************************************************/
class AudioSink : public AudioProcessorBase {
public:
    explicit AudioSink(int32_t channelCount)
            : input(*this, channelCount) {
    }

    virtual ~AudioSink() = default;

    AudioFloatInputPort input;

    /**
     * Dummy processor. The work happens in the read() method.
     *
     * @param framePosition index of first frame to be processed
     * @param numFrames
     * @return number of frames actually processed
     */
    int32_t onProcess(int64_t framePosition, int32_t numFrames) override {
        (void) framePosition;
        (void) numFrames;
        return 0;
    };

    virtual int32_t read(void *data, int32_t numFrames) = 0;

protected:
    int32_t pull(int32_t numFrames);

private:
    int64_t mFramePosition = 0;
};

} /* namespace flowgraph */

#endif /* FLOWGRAPH_AUDIO_PROCESSOR_BASE_H */
