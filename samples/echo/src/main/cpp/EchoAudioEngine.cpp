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

#include "EchoAudioEngine.h"
#include <audio_common.h>
#include <logging_macros.h>
#include <climits>
#include <assert.h>

EchoAudioEngine::EchoAudioEngine() {
  assert(outputChannelCount_ == inputChannelCount_);
  mixerEffect_ = std::unique_ptr<AudioMixer>(new AudioMixer);
}

EchoAudioEngine::~EchoAudioEngine() {
  stopStream(playStream_);
  stopStream(recordingStream_);

  closeStream(playStream_);
  frameCallbackCount_ = 0;

  closeStream(recordingStream_);
}

void EchoAudioEngine::setRecordingDeviceId(int32_t deviceId) {
  recordingDeviceId_ = deviceId;
}

void EchoAudioEngine::setPlaybackDeviceId(int32_t deviceId) {
  playbackDeviceId_ = deviceId;
}

bool EchoAudioEngine::isAAudioSupported() {
  oboe::AudioStreamBuilder builder;
  return builder.isAAudioSupported();
}
bool EchoAudioEngine::setAudioApi(oboe::AudioApi api) {
  if (isEchoOn_)
    return false;

  audioApi_ = api;
  return true;
}
void EchoAudioEngine::setEchoOn(bool isEchoOn) {
  if (isEchoOn != isEchoOn_) {
    isEchoOn_ = isEchoOn;

    if (isEchoOn) {
      openAllStreams();
    } else {
      closeAllStreams();
    }
  }
}

void EchoAudioEngine::openAllStreams() {
  // Note: The order of stream creation is important. We create the playback
  // stream first, then use properties from the playback stream
  // (e.g. sample rate) to create the recording stream. By matching the
  // properties we should get the lowest latency path
  openPlaybackStream();
  openRecordingStream();
  // Now start the recording stream first so that we can read from it during the
  // playback stream's dataCallback
  if (recordingStream_ && playStream_) {
    mixAudio_ = mixerEffect_->AudioFormatSupported(playStream_->getSampleRate(),
      playStream_->getChannelCount(), playStream_->getFormat());

    startStream(recordingStream_);
    startStream(playStream_);
  } else {
    LOGE("Failed to create recording (%p) and/or playback (%p) stream",
         recordingStream_, playStream_);
    closeAllStreams();
  }
}

/**
 * Stops and closes the playback and recording streams.
 */
void EchoAudioEngine::closeAllStreams() {
  /**
   * Note: The order of events is important here.
   * The playback stream must be closed before the recording stream. If the
   * recording stream were to be closed first the playback stream's
   * callback may attempt to read from the recording stream
   * which would cause the app to crash since the recording stream would be
   * null.
   */

  if (playStream_ != nullptr) {
    closeStream(playStream_);  // Calling close will also stop the stream
    playStream_ = nullptr;
  }

  if (recordingStream_ != nullptr) {
    closeStream(recordingStream_);
    recordingStream_ = nullptr;
  }
  mixAudio_ = false;
}

/**
 * Creates an audio stream for recording. The audio device used will depend on
 * recordingDeviceId_.
 * If the value is set to oboe::Unspecified then the default recording device
 * will be used.
 */
void EchoAudioEngine::openRecordingStream() {
  // To create a stream we use a stream builder. This allows us to specify all
  // the parameters for the stream prior to opening it
  oboe::AudioStreamBuilder builder;

  setupRecordingStreamParameters(&builder);

  // Now that the parameters are set up we can open the stream
  oboe::Result result = builder.openStream(&recordingStream_);
  if (result == oboe::Result::OK && recordingStream_) {
    assert(recordingStream_->getChannelCount() == inputChannelCount_);
    assert(recordingStream_->getSampleRate() == sampleRate_);
    assert(recordingStream_->getFormat() == oboe::AudioFormat::I16);

    warnIfNotLowLatency(recordingStream_);
    PrintAudioStreamInfo(recordingStream_);
  } else {
    LOGE("Failed to create recording stream. Error: %s", convertToText(result));
  }
}

/**
 * Creates an audio stream for playback. The audio device used will depend on
 * playbackDeviceId_.
 * If the value is set to oboe::Unspecified then the default playback device
 * will be used.
 */
void EchoAudioEngine::openPlaybackStream() {
  oboe::AudioStreamBuilder builder;

  setupPlaybackStreamParameters(&builder);
  oboe::Result result = builder.openStream(&playStream_);
  if (result == oboe::Result::OK && playStream_) {
    sampleRate_ = playStream_->getSampleRate();

    assert(sampleRate_ == kLoopbackSampleRate);
    assert(playStream_->getFormat() == oboe::AudioFormat::I16);
    assert(outputChannelCount_ == playStream_->getChannelCount());

    framesPerBurst_ = playStream_->getFramesPerBurst();

    // Read blocking timeout value: half of the burst size
    audioBlockingReadTimeout_ = static_cast<uint64_t>(.5f * framesPerBurst_
                                          / sampleRate_ * NANOS_PER_SECOND);

    latencyTuner_ = std::unique_ptr<oboe::LatencyTuner>
                    (new oboe::LatencyTuner(*playStream_));

    delayEffect_ = std::unique_ptr<AudioDelay>(new AudioDelay(
      sampleRate_,outputChannelCount_, format_, echoDelay_, echoDecay_));
    assert(delayEffect_ && mixerEffect_);

    frameCallbackCount_ = 0;
    warnIfNotLowLatency(playStream_);

    PrintAudioStreamInfo(playStream_);
  } else {
    LOGE("Failed to create playback stream. Error: %s",
         oboe::convertToText(result));
  }
}

/**
 * Sets the stream parameters which are specific to recording,
 * including the sample rate which is determined from the
 * playback stream.
 *
 * @param builder The recording stream builder
 */
oboe::AudioStreamBuilder *EchoAudioEngine::setupRecordingStreamParameters(
    oboe::AudioStreamBuilder *builder) {
  // This sample uses blocking read() by setting callback to null
  builder->setCallback(nullptr)
      ->setDeviceId(recordingDeviceId_)
      ->setDirection(oboe::Direction::Input)
      ->setSampleRate(sampleRate_)
      ->setChannelCount(inputChannelCount_);
  return setupCommonStreamParameters(builder);
}

/**
 * Sets the stream parameters which are specific to playback, including device
 * id and the dataCallback function, which must be set for low latency
 * playback.
 * @param builder The playback stream builder
 */
oboe::AudioStreamBuilder *EchoAudioEngine::setupPlaybackStreamParameters(
    oboe::AudioStreamBuilder *builder) {
  builder->setCallback(this)
      ->setDeviceId(playbackDeviceId_)
      ->setDirection(oboe::Direction::Output)
      ->setChannelCount(outputChannelCount_)
      ->setSampleRate(sampleRate_);

  return setupCommonStreamParameters(builder);
}

/**
 * Set the stream parameters which are common to both recording and playback
 * streams.
 * @param builder The playback or recording stream builder
 */
oboe::AudioStreamBuilder *EchoAudioEngine::setupCommonStreamParameters(
    oboe::AudioStreamBuilder *builder) {
  // We request EXCLUSIVE mode since this will give us the lowest possible
  // latency.
  // If EXCLUSIVE mode isn't available the builder will fall back to SHARED
  // mode.
  builder->setAudioApi(audioApi_)
      ->setFormat(format_)
      ->setSharingMode(oboe::SharingMode::Exclusive)
      ->setPerformanceMode(oboe::PerformanceMode::LowLatency);
  return builder;
}

void EchoAudioEngine::startStream(oboe::AudioStream *stream) {
  assert(stream);
  if (stream) {
    oboe::Result result = stream->requestStart();
    if (result != oboe::Result::OK) {
      LOGE("Error starting stream. %s", convertToText(result));
    }
  }
}

void EchoAudioEngine::stopStream(oboe::AudioStream *stream) {
  if (stream) {
    oboe::Result result = stream->start(0L);
    if (result != oboe::Result::OK) {
      LOGE("Error stopping stream. %s", oboe::convertToText(result));
    }
  }
}

/**
 * Close the stream. AudioStream::close() is a blocking call so
 * the application does not need to add synchronization between
 * onAudioReady() function and the thread calling close().
 * [the closing thread is the UI thread in this sample].
 * @param stream the stream to close
 */
void EchoAudioEngine::closeStream(oboe::AudioStream *stream) {
  if (stream) {
    oboe::Result result = stream->close();
    if (result != oboe::Result::OK) {
      LOGE("Error closing stream. %s", convertToText(result));
    }
  }
}

/**
 * Restart the streams. During the restart operation subsequent calls to this
 * method will output a warning.
 */
void EchoAudioEngine::restartStreams() {
  LOGI("Restarting streams");

  if (restartingLock_.try_lock()) {
    closeAllStreams();
    openAllStreams();
    restartingLock_.unlock();
  } else {
    LOGW(
        "Restart stream operation already in progress - ignoring this request");
    // We were unable to obtain the restarting lock which means the restart
    // operation is currently
    // active. This is probably because we received successive "stream
    // disconnected" events.
    // Internal issue b/63087953
  }
}

/**
 * Warn in logcat if non-low latency stream is created
 * @param stream: newly created stream
 *
 */
void EchoAudioEngine::warnIfNotLowLatency(oboe::AudioStream *stream) {
  if (stream->getPerformanceMode() != oboe::PerformanceMode::LowLatency) {
    LOGW(
        "Stream is NOT low latency."
        "Check your requested format, sample rate and channel count");
  }
}

/**
 * Handles playback stream's audio request. In this sample, we simply block-read
 * from the record stream for the required samples.
 *
 * @param oboeStream: the playback stream that requesting additional samples
 * @param audioData:  the buffer to load audio samples for playback stream
 * @param numFrames:  number of frames to load to audioData buffer
 * @return: DataCallbackResult::Continue.
 */
oboe::DataCallbackResult EchoAudioEngine::onAudioReady(
    oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {

  assert(oboeStream == playStream_);

  if (frameCallbackCount_) {
    latencyTuner_->tune();
  }
  frameCallbackCount_++;

  // blocking read with timeout:
  //     recorder may not have data ready, specifically
  //     at the very beginning; in this case, simply play
  //     silent audio. The timeout is equivalent to
  //       framesPerBurst()/2
  //     Do not make it too long, otherwise player would underrun
  //     and if tuning is in process, player will increase
  //     FramesPerBurst.
  oboe::ErrorOrValue<int32_t> status =
    recordingStream_->read(audioData, numFrames, audioBlockingReadTimeout_);

  int32_t framesRead = (!status) ? 0 : status.value();
  if (framesRead < numFrames) {
    int32_t bytesPerFrame = recordingStream_->getChannelCount() *
                            SampleFormatToBpp(oboeStream->getFormat()) / 8;
    uint8_t *padPos = static_cast<uint8_t*>(audioData) +
                      framesRead * bytesPerFrame;
    memset(padPos, 0, (size_t)(numFrames - framesRead) * bytesPerFrame);
  }

  delayEffect_->process(static_cast<int16_t *>(audioData),
                        outputChannelCount_, numFrames);
  if (mixAudio_) {
    mixerEffect_->process(static_cast<int16_t *>(audioData),
                         outputChannelCount_, numFrames);
  }
  return oboe::DataCallbackResult::Continue;
}

/**
 * Oboe notifies the application for "about to close the stream".
 *
 * @param oboeStream: the stream to close
 * @param error: oboe's reason for closing the stream
 */
void EchoAudioEngine::onErrorBeforeClose(oboe::AudioStream *oboeStream,
                                         oboe::Result error) {
  LOGE("%s stream Error before close: %s",
       oboe::convertToText(oboeStream->getDirection()),
       oboe::convertToText(error));
}

/**
 * Oboe notifies application that "the stream is closed"
 *
 * @param oboeStream
 * @param error
 */
void EchoAudioEngine::onErrorAfterClose(oboe::AudioStream *oboeStream,
                                        oboe::Result error) {
  LOGE("%s stream Error after close: %s",
       oboe::convertToText(oboeStream->getDirection()),
       oboe::convertToText(error));
}

void EchoAudioEngine::setBackgroundStream(
      std::unique_ptr<int16_t[]> samples, size_t sampleCount,
      int32_t sampleRate, int32_t channelCount) {

  mixerEffect_->addStream(std::move(samples), sampleCount, sampleRate,
                         channelCount, oboe::AudioFormat::I16);
}

void EchoAudioEngine::setBackgroundMixer(float bgFactor) {
  mixerEffect_->setBackgroundMixer(bgFactor);
}

/**
 *  Configure echo delay and decay value
 *  @param delay: delay in second
 *  @param decay: decay in second
 */
void EchoAudioEngine::setEchoControls(float delay, float decay) {
  echoDelay_ = delay;
  echoDecay_ = decay;
  if (delayEffect_) {
    delayEffect_->setDelay(echoDelay_);
    delayEffect_->setDecay(echoDecay_);
  }
}

