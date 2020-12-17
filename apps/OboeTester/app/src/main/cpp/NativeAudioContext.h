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

#ifndef NATIVEOBOE_NATIVEAUDIOCONTEXT_H
#define NATIVEOBOE_NATIVEAUDIOCONTEXT_H

#include <jni.h>
#include <sys/system_properties.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"

#include "AAudioExtensions.h"
#include "AudioStreamGateway.h"

#include "flowunits/ImpulseOscillator.h"
#include "flowgraph/ManyToMultiConverter.h"
#include "flowgraph/MonoToMultiConverter.h"
#include "flowgraph/SinkFloat.h"
#include "flowgraph/SinkI16.h"
#include "flowunits/ExponentialShape.h"
#include "flowunits/LinearShape.h"
#include "flowunits/SineOscillator.h"
#include "flowunits/SawtoothOscillator.h"
#include "flowunits/TriangleOscillator.h"

#include "FullDuplexAnalyzer.h"
#include "FullDuplexEcho.h"
#include "FullDuplexStream.h"
#include "analyzer/GlitchAnalyzer.h"
#include "analyzer/DataPathAnalyzer.h"
#include "InputStreamCallbackAnalyzer.h"
#include "MultiChannelRecording.h"
#include "OboeStreamCallbackProxy.h"
#include "PlayRecordingCallback.h"
#include "SawPingGenerator.h"

// These must match order in strings.xml and in StreamConfiguration.java
#define NATIVE_MODE_UNSPECIFIED  0
#define NATIVE_MODE_OPENSLES     1
#define NATIVE_MODE_AAUDIO       2

#define MAX_SINE_OSCILLATORS     8
#define AMPLITUDE_SINE           1.0
#define AMPLITUDE_SAWTOOTH       0.5
#define FREQUENCY_SAW_PING       800.0
#define AMPLITUDE_SAW_PING       0.8
#define AMPLITUDE_IMPULSE        0.7

#define NANOS_PER_MICROSECOND    ((int64_t) 1000)
#define NANOS_PER_MILLISECOND    (1000 * NANOS_PER_MICROSECOND)
#define NANOS_PER_SECOND         (1000 * NANOS_PER_MILLISECOND)

#define SECONDS_TO_RECORD        10

/**
 * Abstract base class that corresponds to a test at the Java level.
 */
class ActivityContext {
public:

    ActivityContext() {}

    virtual ~ActivityContext() = default;

    std::shared_ptr<oboe::AudioStream> getStream(int32_t streamIndex) {
        auto it = mOboeStreams.find(streamIndex);
        if (it != mOboeStreams.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    virtual void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder);

    int open(jint nativeApi,
             jint sampleRate,
             jint channelCount,
             jint format,
             jint sharingMode,
             jint performanceMode,
             jint inputPreset,
             jint deviceId,
             jint sessionId,
             jint framesPerBurst,
             jboolean channelConversionAllowed,
             jboolean formatConversionAllowed,
             jint rateConversionQuality,
             jboolean isMMap,
             jboolean isInput);


    virtual void close(int32_t streamIndex);

    virtual void configureForStart() {}

    oboe::Result start();

    oboe::Result pause();

    oboe::Result stopAllStreams();

    virtual oboe::Result stop() {
        return stopAllStreams();
    }

    double getCpuLoad() {
        return oboeCallbackProxy.getCpuLoad();
    }

    void setWorkload(double workload) {
        oboeCallbackProxy.setWorkload(workload);
    }

    virtual oboe::Result startPlayback() {
        return oboe::Result::OK;
    }

    virtual oboe::Result stopPlayback() {
        return oboe::Result::OK;
    }

    virtual void runBlockingIO() {};

    static void threadCallback(ActivityContext *context) {
        context->runBlockingIO();
    }

    void stopBlockingIOThread() {
        if (dataThread != nullptr) {
            // stop a thread that runs in place of the callback
            threadEnabled.store(false); // ask thread to exit its loop
            dataThread->join();
            dataThread = nullptr;
        }
    }

    virtual double getPeakLevel(int index) {
        return 0.0;
    }

    /**
     * Trigger a sound or impulse.
     * @param enabled
     */
    virtual void trigger() {}

    bool isMMapUsed(int32_t streamIndex);

    int32_t getFramesPerBlock() {
        return (callbackSize == 0) ? mFramesPerBurst : callbackSize;
    }

    int64_t getCallbackCount() {
        return oboeCallbackProxy.getCallbackCount();
    }

    oboe::Result getLastErrorCallbackResult() {
        std::shared_ptr<oboe::AudioStream> stream = getOutputStream();
        if (stream == nullptr) {
            stream = getInputStream();
        }
        return stream ? oboe::Result::ErrorNull : stream->getLastErrorCallbackResult();
    }

    int32_t getFramesPerCallback() {
        return oboeCallbackProxy.getFramesPerCallback();
    }

    virtual void setChannelEnabled(int channelIndex, bool enabled) {}

    virtual void setSignalType(int signalType) {}

    virtual int32_t saveWaveFile(const char *filename);

    virtual void setMinimumFramesBeforeRead(int32_t numFrames) {}

    static bool   mUseCallback;
    static int    callbackSize;

protected:
    std::shared_ptr<oboe::AudioStream> getInputStream();
    std::shared_ptr<oboe::AudioStream> getOutputStream();
    int32_t allocateStreamIndex();
    void freeStreamIndex(int32_t streamIndex);

    virtual void createRecording() {
        mRecording = std::make_unique<MultiChannelRecording>(mChannelCount,
                                                             SECONDS_TO_RECORD * mSampleRate);
    }

    virtual void finishOpen(bool isInput, oboe::AudioStream *oboeStream) {}

    virtual oboe::Result startStreams() = 0;

    std::unique_ptr<float []>    dataBuffer{};

    AudioStreamGateway           audioStreamGateway;
    OboeStreamCallbackProxy      oboeCallbackProxy;

    std::unique_ptr<MultiChannelRecording>  mRecording{};

    int32_t                      mNextStreamHandle = 0;
    std::unordered_map<int32_t, std::shared_ptr<oboe::AudioStream>>  mOboeStreams;
    int32_t                      mFramesPerBurst = 0; // TODO per stream
    int32_t                      mChannelCount = 0; // TODO per stream
    int32_t                      mSampleRate = 0; // TODO per stream

    std::atomic<bool>            threadEnabled{false};
    std::thread                 *dataThread = nullptr;

private:
};

/**
 * Test a single input stream.
 */
class ActivityTestInput : public ActivityContext {
public:

    ActivityTestInput() {}
    virtual ~ActivityTestInput() = default;

    void configureForStart() override;

    double getPeakLevel(int index) override {
        return mInputAnalyzer.getPeakLevel(index);
    }

    void runBlockingIO() override;

    InputStreamCallbackAnalyzer  mInputAnalyzer;

    void setMinimumFramesBeforeRead(int32_t numFrames) override {
        mInputAnalyzer.setMinimumFramesBeforeRead(numFrames);
        mMinimumFramesBeforeRead = numFrames;
    }

    int32_t getMinimumFramesBeforeRead() const {
        return mMinimumFramesBeforeRead;
    }

protected:

    oboe::Result startStreams() override {
        mInputAnalyzer.reset();
        return getInputStream()->requestStart();
    }

    int32_t mMinimumFramesBeforeRead = 0;
};

/**
 * Record a configured input stream and play it back some simple way.
 */
class ActivityRecording : public ActivityTestInput {
public:

    ActivityRecording() {}
    virtual ~ActivityRecording() = default;

    oboe::Result stop() override {

        oboe::Result resultStopPlayback = stopPlayback();
        oboe::Result resultStopAudio = ActivityContext::stop();

        return (resultStopPlayback != oboe::Result::OK) ? resultStopPlayback : resultStopAudio;
    }

    oboe::Result startPlayback() override;

    oboe::Result stopPlayback() override;

    PlayRecordingCallback        mPlayRecordingCallback;
    oboe::AudioStream           *playbackStream = nullptr;

};

/**
 * Test a single output stream.
 */
class ActivityTestOutput : public ActivityContext {
public:
    ActivityTestOutput()
            : sineOscillators(MAX_SINE_OSCILLATORS)
            , sawtoothOscillators(MAX_SINE_OSCILLATORS) {}

    virtual ~ActivityTestOutput() = default;

    void close(int32_t streamIndex) override;

    oboe::Result startStreams() override {
        return getOutputStream()->start();
    }

    void configureForStart() override;

    virtual void configureStreamGateway();

    void runBlockingIO() override;

    void setChannelEnabled(int channelIndex, bool enabled) override;

    // WARNING - must match order in strings.xml and OboeAudioOutputStream.java
    enum SignalType {
        Sine = 0,
        Sawtooth = 1,
        FreqSweep = 2,
        PitchSweep = 3,
        WhiteNoise = 4
    };

    void setSignalType(int signalType) override {
        mSignalType = (SignalType) signalType;
    }

protected:
    SignalType                       mSignalType = SignalType::Sine;

    std::vector<SineOscillator>      sineOscillators;
    std::vector<SawtoothOscillator>  sawtoothOscillators;
    static constexpr float           kSweepPeriod = 10.0; // for triangle up and down

    // A triangle LFO is shaped into either a linear or an exponential range.
    TriangleOscillator               mTriangleOscillator;
    LinearShape                      mLinearShape;
    ExponentialShape                 mExponentialShape;

    std::unique_ptr<ManyToMultiConverter>   manyToMulti;
    std::unique_ptr<MonoToMultiConverter>   monoToMulti;
    std::shared_ptr<oboe::flowgraph::SinkFloat>   mSinkFloat;
    std::shared_ptr<oboe::flowgraph::SinkI16>     mSinkI16;
};

/**
 * Generate a short beep with a very short attack.
 * This is used by Java to measure output latency.
 */
class ActivityTapToTone : public ActivityTestOutput {
public:
    ActivityTapToTone() {}
    virtual ~ActivityTapToTone() = default;

    void configureForStart() override;

    virtual void trigger() override {
        sawPingGenerator.trigger();
    }

    SawPingGenerator             sawPingGenerator;
};

/**
 * Activity that uses synchronized input/output streams.
 */
class ActivityFullDuplex : public ActivityContext {
public:

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    virtual int32_t getState() { return -1; }
    virtual int32_t getResult() { return -1; }
    virtual bool isAnalyzerDone() { return false; }

    void setMinimumFramesBeforeRead(int32_t numFrames) override {
        getFullDuplexAnalyzer()->setMinimumFramesBeforeRead(numFrames);
    }

    virtual FullDuplexAnalyzer *getFullDuplexAnalyzer() = 0;

    int32_t getResetCount() {
        return getFullDuplexAnalyzer()->getLoopbackProcessor()->getResetCount();
    }

protected:
    void createRecording() override {
        mRecording = std::make_unique<MultiChannelRecording>(2, // output and input
                                                             SECONDS_TO_RECORD * mSampleRate);
    }
};

/**
 * Echo input to output through a delay line.
 */
class ActivityEcho : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        return mFullDuplexEcho->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    void setDelayTime(double delayTimeSeconds) {
        if (mFullDuplexEcho) {
            mFullDuplexEcho->setDelayTime(delayTimeSeconds);
        }
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexEcho.get();
    }

protected:
    void finishOpen(bool isInput, oboe::AudioStream *oboeStream) override;

private:
    std::unique_ptr<FullDuplexEcho>   mFullDuplexEcho{};
};

/**
 * Measure Round Trip Latency
 */
class ActivityRoundTripLatency : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        mAnalyzerLaunched = false;
        return mFullDuplexLatency->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    LatencyAnalyzer *getLatencyAnalyzer() {
        return &mEchoAnalyzer;
    }

    int32_t getState() override {
        return getLatencyAnalyzer()->getState();
    }

    int32_t getResult() override {
        return getLatencyAnalyzer()->getState(); // TODO This does not look right.
    }

    bool isAnalyzerDone() override {
        if (!mAnalyzerLaunched) {
            mAnalyzerLaunched = launchAnalysisIfReady();
        }
        return mEchoAnalyzer.isDone();
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexLatency.get();
    }

    static void analyzeData(PulseLatencyAnalyzer *analyzer) {
        analyzer->analyze();
    }

    bool launchAnalysisIfReady() {
        // Are we ready to do the analysis?
        if (mEchoAnalyzer.hasEnoughData()) {
            // Crunch the numbers on a separate thread.
            std::thread t(analyzeData, &mEchoAnalyzer);
            t.detach();
            return true;
        }
        return false;
    }

protected:
    void finishOpen(bool isInput, oboe::AudioStream *oboeStream) override;

private:
    std::unique_ptr<FullDuplexAnalyzer>   mFullDuplexLatency{};

    PulseLatencyAnalyzer  mEchoAnalyzer;
    bool                  mAnalyzerLaunched = false;
};

/**
 * Measure Glitches
 */
class ActivityGlitches : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        return mFullDuplexGlitches->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    GlitchAnalyzer *getGlitchAnalyzer() {
        return &mGlitchAnalyzer;
    }

    int32_t getState() override {
        return getGlitchAnalyzer()->getState();
    }

    int32_t getResult() override {
        return getGlitchAnalyzer()->getResult();
    }

    bool isAnalyzerDone() override {
        return mGlitchAnalyzer.isDone();
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexGlitches.get();
    }

protected:
    void finishOpen(bool isInput, oboe::AudioStream *oboeStream) override;

private:
    std::unique_ptr<FullDuplexAnalyzer>   mFullDuplexGlitches{};
    GlitchAnalyzer  mGlitchAnalyzer;
};

/**
 * Measure Data Path
 */
class ActivityDataPath : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        return mFullDuplexDataPath->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    void configureForStart() override {
        std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
        int32_t capacityInFrames = outputStream->getBufferCapacityInFrames();
        int32_t burstInFrames = outputStream->getFramesPerBurst();
        int32_t capacityInBursts = capacityInFrames / burstInFrames;
        int32_t sizeInBursts = std::max(2, capacityInBursts / 2);
        // Set size of buffer to minimize underruns.
        auto result = outputStream->setBufferSizeInFrames(sizeInBursts * burstInFrames);
        LOGD("ActivityDataPath: %s() capacity = %d, burst = %d, size = %d",
             __func__, capacityInFrames, burstInFrames, result.value());
    }

    DataPathAnalyzer *getDataPathAnalyzer() {
        return &mDataPathAnalyzer;
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexDataPath.get();
    }

protected:
    void finishOpen(bool isInput, oboe::AudioStream *oboeStream) override;

private:
    std::unique_ptr<FullDuplexAnalyzer>   mFullDuplexDataPath{};

    DataPathAnalyzer  mDataPathAnalyzer;
};

/**
 * Test a single output stream.
 */
class ActivityTestDisconnect : public ActivityContext {
public:
    ActivityTestDisconnect() {}

    virtual ~ActivityTestDisconnect() = default;

    void close(int32_t streamIndex) override;

    oboe::Result startStreams() override {
        std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
        if (outputStream) {
            return outputStream->start();
        }

        std::shared_ptr<oboe::AudioStream> inputStream = getInputStream();
        if (inputStream) {
            return inputStream->start();
        }
        return oboe::Result::ErrorNull;
    }

    void configureForStart() override;

private:
    std::unique_ptr<SineOscillator>         sineOscillator;
    std::unique_ptr<MonoToMultiConverter>   monoToMulti;
    std::shared_ptr<oboe::flowgraph::SinkFloat>   mSinkFloat;
};

/**
 * Switch between various
 */
class NativeAudioContext {
public:

    ActivityContext *getCurrentActivity() {
        return currentActivity;
    };

    void setActivityType(int activityType) {
        mActivityType = (ActivityType) activityType;
        switch(mActivityType) {
            default:
            case ActivityType::Undefined:
            case ActivityType::TestOutput:
                currentActivity = &mActivityTestOutput;
                break;
            case ActivityType::TestInput:
                currentActivity = &mActivityTestInput;
                break;
            case ActivityType::TapToTone:
                currentActivity = &mActivityTapToTone;
                break;
            case ActivityType::RecordPlay:
                currentActivity = &mActivityRecording;
                break;
            case ActivityType::Echo:
                currentActivity = &mActivityEcho;
                break;
            case ActivityType::RoundTripLatency:
                currentActivity = &mActivityRoundTripLatency;
                break;
            case ActivityType::Glitches:
                currentActivity = &mActivityGlitches;
                break;
            case ActivityType::TestDisconnect:
                currentActivity = &mActivityTestDisconnect;
                break;
            case ActivityType::DataPath:
                currentActivity = &mActivityDataPath;
                break;
        }
    }

    void setDelayTime(double delayTimeMillis) {
        mActivityEcho.setDelayTime(delayTimeMillis);
    }

    ActivityTestOutput           mActivityTestOutput;
    ActivityTestInput            mActivityTestInput;
    ActivityTapToTone            mActivityTapToTone;
    ActivityRecording            mActivityRecording;
    ActivityEcho                 mActivityEcho;
    ActivityRoundTripLatency     mActivityRoundTripLatency;
    ActivityGlitches             mActivityGlitches;
    ActivityDataPath             mActivityDataPath;
    ActivityTestDisconnect       mActivityTestDisconnect;

private:

    // WARNING - must match definitions in TestAudioActivity.java
    enum ActivityType {
        Undefined = -1,
        TestOutput = 0,
        TestInput = 1,
        TapToTone = 2,
        RecordPlay = 3,
        Echo = 4,
        RoundTripLatency = 5,
        Glitches = 6,
        TestDisconnect = 7,
        DataPath = 8,
    };

    ActivityType                 mActivityType = ActivityType::Undefined;
    ActivityContext             *currentActivity = &mActivityTestOutput;

};

#endif //NATIVEOBOE_NATIVEAUDIOCONTEXT_H
