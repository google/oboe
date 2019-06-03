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

#include <trace.h>
#include <inttypes.h>
#include <memory>

#include "shared/Oscillator.h"

#include "PlayAudioEngine.h"
#include "logging_macros.h"
#include "SoundGenerator.h"

constexpr int64_t kNanosPerMillisecond = 1000000; // Use int64_t to avoid overflows in calculations


PlayAudioEngine::PlayAudioEngine() {

    // Initialize the trace functions, this enables you to output trace statements without
    // blocking. See https://developer.android.com/studio/profile/systrace-commandline.html
    Trace::initialize();
    oboe::AudioStreamBuilder builder = oboe::AudioStreamBuilder();
    createPlaybackStream(&builder);
}

/**
 * Creates an audio stream for playback. Takes in a builder pointer which contains stream params
 */
void PlayAudioEngine::createPlaybackStream(oboe::AudioStreamBuilder *builder) {
    oboe::Result result = builder->setSharingMode(oboe::SharingMode::Exclusive)
    ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
    ->setCallback(this)
    ->openManagedStream(mPlayStream);
    if (result == oboe::Result::OK && mPlayStream.get() != nullptr) {
        // Set the buffer size to the burst size - this will give us the minimum possible latency
        mPlayStream->setBufferSizeInFrames(mPlayStream->getFramesPerBurst());

        // TODO: Implement Oboe_convertStreamToText
        // PrintAudioStreamInfo(mPlayStream);
        if (mPlayStream->getFormat() == oboe::AudioFormat::I16){
            // create a buffer of floats which we can render our audio data into
            int conversionBufferSamples = mPlayStream->getBufferCapacityInFrames() * mPlayStream->getChannelCount();
            LOGD("Stream format is 16-bit integers, creating a temporary buffer of %d samples"
                 " for float->int16 conversion", conversionBufferSamples);
            mConversionBuffer = std::make_unique<float[]>(conversionBufferSamples);
        }

        mSoundGenerator = std::make_unique<SoundGenerator>(
                mPlayStream->getSampleRate(),
                mPlayStream->getBufferCapacityInFrames(),
                mPlayStream->getChannelCount()
            );

        // Create a latency tuner which will automatically tune our buffer size.
        mLatencyTuner = std::make_unique<oboe::LatencyTuner>(*mPlayStream);
        // Start the stream - the dataCallback function will start being called
        result = mPlayStream->requestStart();
        if (result != oboe::Result::OK) {
            LOGE("Error starting stream. %s", oboe::convertToText(result));
        }

        mIsLatencyDetectionSupported = (mPlayStream->getTimestamp(CLOCK_MONOTONIC, 0, 0) !=
                                        oboe::Result::ErrorUnimplemented);

    } else {
        LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
    }
}

void PlayAudioEngine::setToneOn(bool isToneOn) {
    mSoundGenerator->setTonesOn(isToneOn);
}

/**
 * Every time the playback stream requires data this method will be called.
 *
 * @param audioStream the audio stream which is requesting data, this is the mPlayStream object
 * @param audioData an empty buffer into which we can write our audio data
 * @param numFrames the number of audio frames which are required
 * @return Either oboe::DataCallbackResult::Continue if the stream should continue requesting data
 * or oboe::DataCallbackResult::Stop if the stream should stop.
 */
oboe::DataCallbackResult
PlayAudioEngine::onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {

    if (mBufferSizeSelection == kBufferSizeAutomatic) mLatencyTuner->tune();
    int32_t bufferSize = audioStream->getBufferSizeInFrames();

    /**
     * The following output can be seen by running a systrace. Tracing is preferable to logging
     * inside the callback since tracing does not block.
     *
     * See https://developer.android.com/studio/profile/systrace-commandline.html
     */
    auto underrunCountResult = audioStream->getXRunCount();

    if (Trace::isEnabled()) Trace::beginSection("numFrames %d, Underruns %d, buffer size %d",
                        numFrames, underrunCountResult.value(), bufferSize);

    bool is16BitFormat = (audioStream->getFormat() == oboe::AudioFormat::I16);
    int32_t channelCount = audioStream->getChannelCount();

    // If the stream is 16-bit render into a float buffer then convert that buffer to 16-bit ints
    float *outputBuffer = (is16BitFormat) ? mConversionBuffer.get() : static_cast<float *>(audioData);
    mSoundGenerator->renderAudio(outputBuffer, numFrames);

    if (is16BitFormat){
        oboe::convertFloatToPcm16(outputBuffer,
                                  static_cast<int16_t *>(audioData),
                                  numFrames * channelCount);
    }

    if (mIsLatencyDetectionSupported) {
        calculateCurrentOutputLatencyMillis(audioStream, &mCurrentOutputLatencyMillis);
    }

    if (Trace::isEnabled()) Trace::endSection();
    return oboe::DataCallbackResult::Continue;
}

/**
 * Calculate the current latency between writing a frame to the output stream and
 * the same frame being presented to the audio hardware.
 *
 * Here's how the calculation works:
 *
 * 1) Get the time a particular frame was presented to the audio hardware
 * @see AudioStream::getTimestamp
 * 2) From this extrapolate the time which the *next* audio frame written to the stream
 * will be presented
 * 3) Assume that the next audio frame is written at the current time
 * 4) currentLatency = nextFramePresentationTime - nextFrameWriteTime
 *
 * @param stream The stream being written to
 * @param latencyMillis pointer to a variable to receive the latency in milliseconds between
 * writing a frame to the stream and that frame being presented to the audio hardware.
 * @return oboe::Result::OK or a oboe::Result::Error* value. It is normal to receive an error soon
 * after a stream has started because the timestamps are not yet available.
 */
oboe::Result
PlayAudioEngine::calculateCurrentOutputLatencyMillis(oboe::AudioStream *stream,
                                                     double *latencyMillis) {

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
        *latencyMillis = static_cast<double>(nextFramePresentationTime - nextFrameWriteTime)
                         / kNanosPerMillisecond;
    } else {
        LOGE("Error calculating latency: %s", oboe::convertToText(result.error()));
    }

    return result;
}

/**
 * If there is an error with a stream this function will be called. A common example of an error
 * is when an audio device (such as headphones) is disconnected. It is safe to restart the stream
 * in this method. There is no need to create a new thread.
 *
 * @param audioStream the stream with the error
 * @param error the error which occured, a human readable string can be obtained using
 * oboe::convertToText(error);
 *
 * @see oboe::StreamCallback
 */
void PlayAudioEngine::onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) {
    if (error == oboe::Result::ErrorDisconnected) {
        oboe::AudioStreamBuilder builder_ = oboe::AudioStreamBuilder(*oboeStream);
        restartStream(&builder_);
    }
}

void PlayAudioEngine::restartStream(oboe::AudioStreamBuilder *builder) {
    LOGI("Restarting stream");
    createPlaybackStream(builder);
}

double PlayAudioEngine::getCurrentOutputLatencyMillis() {
    return mCurrentOutputLatencyMillis;
}

void PlayAudioEngine::setBufferSizeInBursts(int32_t numBursts) {
    mBufferSizeSelection = numBursts;
    auto result = mPlayStream->setBufferSizeInFrames(
            mBufferSizeSelection * mPlayStream->getFramesPerBurst());
    if (result) {
        LOGD("Buffer size successfully changed to %d", result.value());
    } else {
        LOGW("Buffer size could not be changed, %d", result.error());
    }
}

bool PlayAudioEngine::isLatencyDetectionSupported() {
    return mIsLatencyDetectionSupported;
}
void PlayAudioEngine::setAudioApi(oboe::AudioApi audioApi) {
    oboe::AudioStreamBuilder *builder = oboe::AudioStreamBuilder(*mPlayStream).setAudioApi(audioApi);
    restartStream(builder);
    LOGD("AudioAPI is now %d", mPlayStream->getAudioApi());
}

void PlayAudioEngine::setChannelCount(int channelCount) {
    oboe::AudioStreamBuilder *builder = oboe::AudioStreamBuilder(*mPlayStream).setChannelCount(channelCount);
    restartStream(builder);
    LOGD("Channel count is now %d", mPlayStream->getChannelCount());
}

/**
 * Set the audio device which should be used for playback. Can be set to oboe::kUnspecified if
 * you want to use the default playback device (which is usually the built-in speaker if
 * no other audio devices, such as headphones, are attached).
 *
 * @param deviceId the audio device id, can be obtained through an {@link AudioDeviceInfo} object
 * using Java/JNI.
 */
void PlayAudioEngine::setDeviceId(int32_t deviceId) {
    oboe::AudioStreamBuilder *builder = oboe::AudioStreamBuilder(*mPlayStream).setDeviceId(deviceId);
    restartStream(builder);
    LOGD("Device ID is now %d", mPlayStream->getDeviceId());
}

