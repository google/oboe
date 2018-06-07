/*
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

#ifndef OBOE_ECHOAUDIOENGINE_H
#define OBOE_ECHOAUDIOENGINE_H

#include <thread>
#include <jni.h>
#include <string>
#include "audio_common.h"
#include "AudioEffect.h"

const int32_t kLoopbackSampleRate = 48000;

class EchoAudioEngine : public oboe::AudioStreamCallback {
 public:
  EchoAudioEngine();
  ~EchoAudioEngine();
  void setRecordingDeviceId(int32_t deviceId);
  void setPlaybackDeviceId(int32_t deviceId);
  void setEchoOn(bool isEchoOn);

  /*
   * oboe::AudioStreamCallback interface implementation
   */
  oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream,
                                        void *audioData, int32_t numFrames);
  void onErrorBeforeClose(oboe::AudioStream *oboeStream, oboe::Result error);
  void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error);

  /*
   * handle fileStream
   */
  void setBackgroundStream(std::unique_ptr<int16_t[]>samples, size_t sampleCount,
                           int32_t sampleRate, int32_t channelCount);
  void setBackgroundMixer(float bgFactor);
  void setEchoControls(float delay, float decay);
  bool setAudioApi(oboe::AudioApi);
  bool isAAudioSupported(void);

 private:
  bool isEchoOn_ = false;
  uint64_t frameCallbackCount_ = 0;
  int32_t recordingDeviceId_ = oboe::kUnspecified;
  int32_t playbackDeviceId_ = oboe::kUnspecified;
  oboe::AudioFormat format_ = oboe::AudioFormat::I16;
  int32_t sampleRate_ = kLoopbackSampleRate;
  int32_t inputChannelCount_ = kStereoChannelCount;
  int32_t outputChannelCount_ = kStereoChannelCount;
  oboe::AudioStream *recordingStream_ = nullptr;
  oboe::AudioStream *playStream_ = nullptr;
  int32_t framesPerBurst_;
  std::mutex restartingLock_;
  std::unique_ptr<AudioMixer> mixerEffect_ = nullptr;
  std::unique_ptr<AudioDelay> delayEffect_ = nullptr;
  uint64_t audioBlockingReadTimeout_ = NANOS_PER_MILLISECOND;
  oboe::AudioApi audioApi_ = oboe::AudioApi::AAudio;
  float echoDelay_ = 0.5f;
  float echoDecay_ = 0.1f;

  bool mixAudio_ = false;
  std::unique_ptr<oboe::LatencyTuner> latencyTuner_;

  void openRecordingStream();
  void openPlaybackStream();

  void startStream(oboe::AudioStream *stream);
  void stopStream(oboe::AudioStream *stream);
  void closeStream(oboe::AudioStream *stream);

  void openAllStreams();
  void closeAllStreams();
  void restartStreams();

  oboe::AudioStreamBuilder *setupCommonStreamParameters(
      oboe::AudioStreamBuilder *builder);
  oboe::AudioStreamBuilder *setupRecordingStreamParameters(
      oboe::AudioStreamBuilder *builder);
  oboe::AudioStreamBuilder *setupPlaybackStreamParameters(
      oboe::AudioStreamBuilder *builder);
  void warnIfNotLowLatency(oboe::AudioStream *stream);
};

#endif  // ECHO_ECHOAUDIOENGINE_H
