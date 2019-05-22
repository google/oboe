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
#include <memory>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>

// TODO Move these classes into separate files.
// TODO Maybe remove "Audio" prefix from these class names: AudioProcessorBase to ProcessorNode
// TODO Review use of raw pointers for connect(). Maybe use smart pointers but need to avoid
//      run-time deallocation in audio thread.

// Set this to 1 if using it inside the Android framework.
// This code is kept here so that it can be moved easily between Oboe and AAudio.
#define FLOWGRAPH_ANDROID_INTERNAL 0

namespace flowgraph {

// Default block size that can be overridden when the AudioFloatBufferPort is created.
// If it is too small then we will have too much overhead from switching between nodes.
// If it is too high then we will thrash the caches.
constexpr int kDefaultBufferSize = 8; // arbitrary

class AudioPort;
class AudioFloatInputPort;

/***************************************************************************/
/**
 * Base class for all nodes in the flowgraph.
 */
class AudioProcessorBase {
public:
    virtual ~AudioProcessorBase() = default;

    /**
     * Read from the input ports,
     * generate multiple frames of data then write the results to the output ports.
     *
     * @param numFrames maximum number of frames requested for processing
     * @return number of frames actually processed
     */
    virtual int32_t onProcess(int32_t numFrames) = 0;

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

//    virtual void start() {}
//
//    virtual void stop() {}

    void addInputPort(AudioPort &port) {
        mInputPorts.push_back(port);
    }

protected:
    int64_t  mLastFramePosition = -1; // Start at -1 so that the first pull works.

    std::vector<std::reference_wrapper<AudioPort>> mInputPorts;

private:
    int32_t  mFramesValid = 0; // num valid frames in the block
};

/***************************************************************************/
/**
  * This is a connector that allows data to flow between modules.
  *
  * The ports are the primary means of interacting with a module.
  * So they are generally declared as public.
  *
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

    virtual int32_t pullData(int64_t framePosition, int32_t numFrames) = 0;

protected:
    AudioProcessorBase &mParent;

private:
    const int32_t    mSamplesPerFrame = 1;
};

/***************************************************************************/
/**
 * This port contains a 32-bit float buffer that can contain several frames of data.
 * Processing the data in a block improves performance.
 *
 * The size is framesPerBuffer * samplesPerFrame).
 */
class AudioFloatBufferPort  : public AudioPort {
public:
    AudioFloatBufferPort(AudioProcessorBase &parent,
                   int32_t samplesPerFrame,
                   int32_t framesPerBuffer = kDefaultBufferSize
                );

    virtual ~AudioFloatBufferPort() = default;

    int32_t getFramesPerBuffer() const {
        return mFramesPerBuffer;
    }

protected:

    /**
     * @return buffer internal to the port or from a connected port
     */
    virtual float *getBuffer() {
        return mBuffer.get();
    }

private:
    const int32_t    mFramesPerBuffer = 1;
    std::unique_ptr<float[]> mBuffer; // allocated in constructor
};

/***************************************************************************/
/**
  * The results of a node's processing are stored in the buffers of the output ports.
  */
class AudioFloatOutputPort : public AudioFloatBufferPort {
public:
    AudioFloatOutputPort(AudioProcessorBase &parent, int32_t samplesPerFrame)
            : AudioFloatBufferPort(parent, samplesPerFrame) {
    }

    virtual ~AudioFloatOutputPort() = default;

    using AudioFloatBufferPort::getBuffer;

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

    /**
     * Call the parent module's onProcess() method.
     * That may pull data from its inputs and recursively
     * process the entire graph.
     * @return number of frames actually pulled
     */
    int32_t pullData(int64_t framePosition, int32_t numFrames) override;

};

/***************************************************************************/

/**
 * An input port for streaming audio data.
 * You can set a value that will be used for processing.
 * If you connect an output port to this port then its value will be used instead.
 */
class AudioFloatInputPort : public AudioFloatBufferPort {
public:
    AudioFloatInputPort(AudioProcessorBase &parent, int32_t samplesPerFrame)
            : AudioFloatBufferPort(parent, samplesPerFrame) {
        // Add to parent so it can pull data from each input.
        parent.addInputPort(*this);
    }

    virtual ~AudioFloatInputPort() = default;

    /**
     * If connected to an output port then this will return
     * that output ports buffers.
     * If not connected then it returns the input ports own buffer
     * which can be loaded using setValue().
     */
    float *getBuffer() override;

    /**
     * Write every value of the float buffer.
     * This value will be ignored if an output port is connected
     * to this port.
     */
    void setValue(float value) {
        int numFloats = kDefaultBufferSize * getSamplesPerFrame();
        float *buffer = getBuffer();
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

    /**
     * Pull data from any output port that is connected.
     */
    int32_t pullData(int64_t framePosition, int32_t numFrames) override;

private:
    AudioFloatOutputPort *mConnected = nullptr;
};

/***************************************************************************/

/**
 * Base class for an edge node in a graph that has no upstream nodes.
 * It outputs data but does not consume data.
 * By default, it will read its data from an external buffer.
 */
class AudioSource : public AudioProcessorBase {
public:
    explicit AudioSource(int32_t channelCount)
            : output(*this, channelCount) {
    }

    virtual ~AudioSource() = default;

    AudioFloatOutputPort output;

    /**
     * Specify buffer that the node will read from.
     *
     * @param data TODO Consider using std::shared_ptr.
     * @param numFrames
     */
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
/**
 * Base class for an edge node in a graph that has no downstream nodes.
 * It consumes data but does not output data.
 * This graph will be executed when data is read() from this node
 * by pulling data from upstream nodes.
 */
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
     * @param numFrames
     * @return number of frames actually processed
     */
    int32_t onProcess(int32_t numFrames) override {
        return numFrames;
    }

    virtual int32_t read(int64_t framePosition, void *data, int32_t numFrames) = 0;

};

} /* namespace flowgraph */

#endif /* FLOWGRAPH_AUDIO_PROCESSOR_BASE_H */
