/*
 * Copyright 2018 The Android Open Source Project
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
#include "AudioEffect.h"
#include <logging_macros.h>
#include <climits>
#include <cstring>
#include <audio_common.h>

/*
 * Mixing Audio in integer domain to avoid FP calculation
 *   (FG * ( MixFactor * 16 ) + BG * ( (1.0f-MixFactor) * 16 )) / 16
 */
static const int32_t kFloatToIntMapFactor = 128;

AudioMixer::AudioMixer() :
    AudioFormat(48000, 2, oboe::AudioFormat::I16) {

  busy_ = false;
  bgMixFactorInt_ = (int32_t)
                   (bgMixFactor_ * kFloatToIntMapFactor + 0.5f);
  fgMixFactorInt_ = kFloatToIntMapFactor - bgMixFactorInt_;
}

/**
 * destructor: release memory for audio samples
 */
AudioMixer::~AudioMixer() {
}
/**
 * Set mix factor for the 2 streams( background and forground );
 * blending:
 *    recordedAudio * fgMix +
 *    backgroundMusic * ( 1.0f - fgMix )
 * @param fgMix is background music mixer
 */
void  AudioMixer::setBackgroundMixer(float mixer) {
  if (mixer >= 0.0f && mixer <= 1.0f) {
    bgMixFactor_ = mixer;
    bgMixFactorInt_ = (int32_t)
      (bgMixFactor_ * kFloatToIntMapFactor + 0.5f);
    fgMixFactorInt_ = kFloatToIntMapFactor - bgMixFactorInt_;
  }
}

/**
 * Insert a raw PCM audio buffer to blend with live audio
 *
 * @param samples points to PCM audio buffer
 * @param sampleCount is total samples pointed by samples
 * @param channelCount channels for PCM audio pointed by samples
 * @param freq is PCM audio frequency (48000hz for this sample)
 */
void  AudioMixer::addStream(std::unique_ptr<int16_t[]>samples, size_t sampleCount,
          int32_t sampleRate, int32_t channelCount, oboe::AudioFormat format){
  if (busy_) {
    LOGW("filtering in progress, filter configuration is IGNORED");
    return;
  }
  bgAudio_ = std::move(samples);
  bgAudioSampleCount_ = sampleCount;
  sampleRate_ = sampleRate;
  format_ = format;
  channelCount_ = channelCount;

  curPosition_ = 0;
}

/**
 * Adding audio processing into the live audio
 * @param liveAudio is recorded audio
 * @param samplesPerFrame is same as channelCount.
 * @param numFrames represents frames pointed by liveAudio
 *        total samples = numFrames * samplesPerFrame
 */
void AudioMixer::process(int16_t *liveAudio, int32_t channelCount,
                          int32_t numFrames) {
  assert(bgAudio_ && liveAudio);
  if (numFrames > bgAudioSampleCount_ || channelCount != channelCount_ ||
      bgMixFactorInt_ == 0) {
    return;
  }

  busy_ = true;
  int32_t curSample;
  for (int i = 0; i < (numFrames * channelCount); i++) {
    curSample = liveAudio[i];
    curSample = curSample * fgMixFactorInt_ +
                bgAudio_[curPosition_] * bgMixFactorInt_;
    curSample /= kFloatToIntMapFactor;

    curSample = (curSample > SHRT_MAX ? SHRT_MAX : curSample);
    liveAudio[i] = (int16_t)(curSample < SHRT_MIN ? SHRT_MIN : curSample);
    curPosition_ = (curPosition_ + 1 ) % bgAudioSampleCount_;
  }
  busy_ = false;
}

/**
 * query for audio format supportability
 */
bool AudioMixer::AudioFormatSupported(int32_t frequency,
                  int32_t channels, oboe::AudioFormat format) const {
  return (frequency == sampleRate_ &&
          channels == channelCount_ &&
          format == format_);
}

/**
 * Constructor for AudioDelay
 * @param sampleRate
 * @param channelCount
 * @param format
 * @param delay
 * @param decay
 */
AudioDelay::AudioDelay(int32_t sampleRate,
                       int32_t channelCount,
                       oboe::AudioFormat format,
                       float  delay,
                       float  decay) :
    AudioFormat(sampleRate, channelCount, format),
    delay_(delay), decay_(decay) {

  feedbackFactor_ = static_cast<int32_t>(decay_ * kFloatToIntMapFactor);
  liveAudioFactor_ = kFloatToIntMapFactor - feedbackFactor_;
  allocateBuffer();
}

/**
 * Destructor
 */
AudioDelay::~AudioDelay() {
  if(buffer_) delete static_cast<uint8_t*>(buffer_);
}

/**
 * Configure for delay time ( in miliseconds ). It is possible to dynamically
 * adjust the value
 * @param delay in seconds
 * @return true if delay time is set successfully
 */
bool AudioDelay::setDelay(float delay) {
  float delta = delay - delay_;
  if ( delta > -0.022f && delta < 0.022f) {
    return true;
  }

  std::lock_guard<std::mutex> lock(lock_);

  if(buffer_) {
    delete static_cast<uint8_t*>(buffer_);
    buffer_ = nullptr;
  }

  delay_  = delay;
  allocateBuffer();
  return buffer_ != nullptr;
}

/**
 * Internal helper function to allocate buffer for the delay
 *  - calculate the buffer size for the delay time
 *  - allocate and zero out buffer (0 means silent audio)
 *  - configure bufSize_ to be size of audioFrames
 */
void AudioDelay::allocateBuffer(void) {

  float fNumFrames = sampleRate_ * delay_;

  size_t sampleCount = static_cast<uint32_t>(fNumFrames + 0.5f) * channelCount_;

  uint32_t bytePerSample = SampleFormatToBpp(format_) / 8;
  assert(bytePerSample <= 4);

  uint32_t bytePerFrame =  channelCount_ * bytePerSample;

  // get bufCapacity in bytes
  bufCapacity_ = sampleCount * bytePerSample;
  bufCapacity_ = ((bufCapacity_ + bytePerFrame - 1) / bytePerFrame) * bytePerFrame;

  buffer_ = new uint8_t[bufCapacity_];
  assert(buffer_);

  memset(buffer_, 0, bufCapacity_);
  curPos_ = 0;

  // bufSize_ is in Frames ( not samples, not bytes )
  bufSize_ = bufCapacity_ / bytePerFrame;
}

float AudioDelay::getDelay(void) const {
  return delay_;
}

/**
 * SetFeedbackRatio(): set the decay factor
 * ratio: value of 0.0 -- 1.0f;
 *
 * the calculation is in integer ( not in float )
 * for performance purpose
 */
void AudioDelay::setDecay(float decay) {
  if (decay > 0.0f && decay < 1.0f) {
    float feedback = (decay * kFloatToIntMapFactor + 0.5f);
    feedbackFactor_ = static_cast<int32_t>(feedback);
    liveAudioFactor_ = kFloatToIntMapFactor - feedbackFactor_;
  }
}

float AudioDelay::getDecay(float) const {
  return decay_;
}

/**
 * process() filter live audio with "echo" effect:
 *   delay time is run-time adjustable
 *   decay time could also be adjustable, but not used
 *   in this sample, hardcoded to .5
 *
 * @param liveAudio is recorded audio stream
 * @param channelCount for liveAudio, must be 2 for stereo
 * @param numFrames is length of liveAudio in Frames ( not in byte )
 */
void AudioDelay::process(int16_t *liveAudio,
                         int32_t channelCount,
                         int32_t numFrames) {
  if (feedbackFactor_ == 0 ||
      channelCount != channelCount_ ||
      bufSize_ < numFrames) {
    return;
  }

  if(!lock_.try_lock()) {
    return;
  }

  if (numFrames + curPos_  > bufSize_) {
    curPos_ = 0;
  }

  // process every sample
  int32_t sampleCount = channelCount * numFrames;
  int16_t* samples =  & static_cast<int16_t*>(buffer_)[curPos_];
  int32_t curSample;
  for (size_t idx = 0; idx < sampleCount; idx++) {
#if 1
    curSample = (samples[idx] * feedbackFactor_ +
                liveAudio[idx] * liveAudioFactor_) / kFloatToIntMapFactor;
#else
    curSample = (samples[idx] * feedbackFactor_) / kFloatToIntMapFactor +
                 liveAudio[idx];
#endif
    if(curSample > SHRT_MAX)
      curSample = SHRT_MAX;
    else if (curSample < SHRT_MIN)
      curSample = SHRT_MAX;
    samples[idx] = static_cast<int16_t>(curSample);
    liveAudio[idx] = samples[idx];
  }

  curPos_ += numFrames;
  lock_.unlock();
}

