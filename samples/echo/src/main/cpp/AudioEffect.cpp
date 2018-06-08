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

#define CLAMP_FOR_I16(x) ((x)>SHRT_MAX ? SHRT_MAX : \
                         ((x)<SHRT_MIN ? SHRT_MIN : (x)))

/*
 * Mixing Audio in integer domain to avoid FP calculation
 *   (FG * ( MixFactor * 16 ) + BG * ( (1.0f-MixFactor) * 16 )) / 16
 */
static const int32_t kFloatToIntMapFactor = 128;

AudioMixer::AudioMixer() :
    AudioFormat(48000, 2, oboe::AudioFormat::I16) {

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
    if (bgMixFactor_ < 0.05f) {
      bgMixFactor_ = 0.0f;
      bgMixFactorInt_ = 0;
    } else {
      bgMixFactorInt_ = (int32_t)
        (bgMixFactor_ * kFloatToIntMapFactor + 0.5f);
    }
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
bool  AudioMixer::addStream(std::unique_ptr<int16_t[]>samples, size_t sampleCount,
          int32_t sampleRate, int32_t channelCount, oboe::AudioFormat format) {
  // Wait for lock, from user context.
  std::lock_guard<std::mutex> lock(lock_);
  bgAudio_ = std::move(samples);
  bgAudioSampleCount_ = sampleCount;
  sampleRate_ = sampleRate;
  format_ = format;
  channelCount_ = channelCount;

  curPosition_ = 0;

  return true;
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
  if(!bgAudio_ || !liveAudio) {
    return;
  }

  if ((numFrames * channelCount) > bgAudioSampleCount_ ||
      channelCount != channelCount_ ||
      bgMixFactorInt_ == 0) {
    return;
  }

  if (!lock_.try_lock()) {
    // UI thread still updating the stream, skip blending
    return;
  }

  size_t sampleCount  = numFrames * channelCount;
  int32_t curSample;
  for (int i = 0; i < sampleCount ; i++) {
    curSample = liveAudio[i];
    curSample = curSample * fgMixFactorInt_ +
                bgAudio_[curPosition_] * bgMixFactorInt_;
    curSample /= kFloatToIntMapFactor;

    curSample = CLAMP_FOR_I16(curSample);
    liveAudio[i] = (int16_t)curSample;
    curPosition_ = (curPosition_ + 1 ) % bgAudioSampleCount_;
  }

  lock_.unlock();
}

/**
 * query for audio format supportability
 */
bool AudioMixer::AudioFormatSupported(int32_t sampleRate,
                  int32_t channels, oboe::AudioFormat format) {
  if (sampleRate != sampleRate_ || format != format_) {
    return false;
  }

  if (channels  == channelCount_ ) {
    return true;
  }

  if(channelCount_ == channels * 2) {
    size_t dst = 0, src = 0;
    size_t totalFrames = bgAudioSampleCount_ / channelCount_;
    for(size_t frame = 0; frame < totalFrames; frame++) {
      for (int32_t c = 0; c < channelCount_; c += 2) {
        int32_t sample = bgAudio_[src] + bgAudio_[src + 1];
        src += 2;
        sample /= 2;
        bgAudio_[dst++] = static_cast<int16_t>(CLAMP_FOR_I16(sample));
      }
    }
    channelCount_ >>= 1;
    bgAudioSampleCount_ >>= 1;
    return true;
  }

  return false;
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
  delete buffer_;
}

/**
 * Configure for delay time ( in second ). It is possible to dynamically
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

  delete (buffer_);

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

  if (buffer_) {
    memset(buffer_, 0, bufCapacity_);
  }
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

  if (!buffer_ || !liveAudio ||
      feedbackFactor_ == 0 ||
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
  int16_t* samples =  & reinterpret_cast<int16_t*>(buffer_)[curPos_ * channelCount_];
  for (size_t idx = 0; idx < sampleCount; idx++) {
    int32_t curSample = (samples[idx] * feedbackFactor_ +
                liveAudio[idx] * liveAudioFactor_) / kFloatToIntMapFactor;
    CLAMP_FOR_I16(curSample);
    liveAudio[idx] = samples[idx];
    samples[idx] = static_cast<int16_t>(curSample);
  }

  curPos_ += numFrames;
  lock_.unlock();
}

