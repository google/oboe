/*
 * Copyright 2025 The Android Open Source Project
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

#include "AudioWorkloadTest.h"

int32_t AudioWorkloadTest::open() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(2);
    builder.setDataCallback(this);

    oboe::Result result = builder.openStream(mStream);
    if (result != oboe::Result::OK) {
        std::cerr << "Error opening stream: " << oboe::convertToText(result) << std::endl;
        return static_cast<int32_t>(result);
    }

    mFramesPerBurst = mStream->getFramesPerBurst();
    mSampleRate = mStream->getSampleRate();
    mPreviousXRunCount = 0;
    mXRunCount = 0;
    mPhaseIncrement = 2.0f * (float) M_PI * 440.0f / mSampleRate; // 440 Hz sine wave

    return 0;
}

int32_t AudioWorkloadTest::getFramesPerBurst() const {
    return mFramesPerBurst;
}

int32_t AudioWorkloadTest::getSampleRate() const {
    return mSampleRate;
}

int32_t AudioWorkloadTest::getBufferSizeInFrames() const {
    return mBufferSizeInFrames;
}

int32_t AudioWorkloadTest::start(int32_t targetDurationMs, int32_t numBursts, int32_t numVoices,
                                 int32_t alternateNumVoices, int32_t alternatingPeriodMs, bool adpfEnabled,
                                 bool hearWorkload) {
    mTargetDurationMs = targetDurationMs;
    mNumBursts = numBursts;
    mNumVoices = numVoices;
    mAlternateNumVoices = alternateNumVoices;
    mAlternatingPeriodMs = alternatingPeriodMs;
    mStartTimeMs = 0;
    mCallbackStatistics.clear();
    mCallbackCount = 0;
    mPreviousXRunCount = mXRunCount.load();
    mXRunCount = 0;
    mRunning = true;
    mHearWorkload = hearWorkload;
    mStream->setPerformanceHintEnabled(adpfEnabled);
    mStream->setBufferSizeInFrames(mNumBursts * mFramesPerBurst);
    mBufferSizeInFrames = mStream->getBufferSizeInFrames();
    mSynthWorkload = SynthWorkload((int) 0.2 * mSampleRate, (int) 0.3 * mSampleRate);
    oboe::Result result = mStream->start();
    if (result != oboe::Result::OK) {
        std::cerr << "Error starting stream: " << oboe::convertToText(result) << std::endl;
        return static_cast<int32_t>(result);
    }

    return 0;
}

int32_t AudioWorkloadTest::setCpuAffinityForCallback(uint32_t mask) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (uint32_t i = 0; i < 32; ++i) {
        if ((mask >> i) & 1) {
            CPU_SET(i, &cpuset);
        }
    }

    if (sched_setaffinity(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Error setting CPU affinity." << std::endl;
        return -1;
    }
    return 0;
}

int32_t AudioWorkloadTest::getCpuCount() {
    return sysconf(_SC_NPROCESSORS_CONF);
}

int32_t AudioWorkloadTest::getXRunCount() const {
    return mXRunCount - mPreviousXRunCount;
}

int32_t AudioWorkloadTest::getCallbackCount() const {
    return mCallbackCount;
}

int64_t AudioWorkloadTest::getLastDurationNs() {
    return mLastDurationNs;
}

bool AudioWorkloadTest::isRunning() {
    return mRunning;
}

int32_t AudioWorkloadTest::stop() {
    if (mStream) {
        oboe::Result result = mStream->stop();
        if (result != oboe::Result::OK) {
            std::cerr << "Error stopping stream: " << oboe::convertToText(result) << std::endl;
            return static_cast<int32_t>(result);
        }
    }
    return 0;
}

int32_t AudioWorkloadTest::close() {
    if (mStream) {
        oboe::Result result = mStream->close();
        mStream = nullptr;
        if (result != oboe::Result::OK) {
            std::cerr << "Error closing stream: " << oboe::convertToText(result) << std::endl;
            return static_cast<int32_t>(result);
        }
    }
    return 0;
}

std::vector<AudioWorkloadTest::CallbackStatus> AudioWorkloadTest::getCallbackStatistics() {
    return mCallbackStatistics;
}

oboe::DataCallbackResult AudioWorkloadTest::onAudioReady(oboe::AudioStream* audioStream,
                                                         void* audioData, int32_t numFrames) {
    int64_t beginTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    int currentVoices = mNumVoices;
    if (mAlternatingPeriodMs > 0) {
        int64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        if (mStartTimeMs == 0) {
            mStartTimeMs = timeMs;
        }
        if (((timeMs - mStartTimeMs) % (2 * mAlternatingPeriodMs)) >= mAlternatingPeriodMs) {
            currentVoices = mAlternateNumVoices;
        }
    }

    auto floatData = static_cast<float *>(audioData);
    int channelCount = audioStream->getChannelCount();

    // Fill buffer with a sine wave.
    for (int i = 0; i < numFrames; i++) {
        float value = sinf(mPhase) * 0.2f;
        for (int j = 0; j < channelCount; j++) {
            *floatData++ = value;
        }
        mPhase = mPhase + mPhaseIncrement;
        // Wrap the phase around in a circle.
        if (mPhase >= M_PI) mPhase = mPhase - 2.0f * M_PI;
    }

    mSynthWorkload.onCallback(currentVoices);
    if (currentVoices > 0) {
        // Render synth workload into the buffer or discard the synth voices.
        float *buffer = (audioStream->getChannelCount() == 2 && mHearWorkload)
                        ? static_cast<float *>(audioData) : nullptr;
        mSynthWorkload.renderStereo(buffer, numFrames);
    }

    int64_t finishTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    mXRunCount = audioStream->getXRunCount().value();

    CallbackStatus status{};
    status.numVoices = currentVoices;
    status.beginTimeNs = beginTimeNs;
    status.finishTimeNs = finishTimeNs;
    status.xRunCount = mXRunCount - mPreviousXRunCount;
    status.cpuIndex = sched_getcpu();

    mCallbackStatistics.push_back(status);
    mCallbackCount++;
    mLastDurationNs = finishTimeNs - beginTimeNs;

    int64_t currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    if (currentTimeMs - mStartTimeMs > mTargetDurationMs) {
        mRunning = false;
        stop();
        return oboe::DataCallbackResult::Stop;
    }

    return oboe::DataCallbackResult::Continue;
}
