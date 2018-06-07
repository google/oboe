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

#ifndef EFFECT_PROCESSOR_H
#define EFFECT_PROCESSOR_H

#include <cstdint>
#include <atomic>
#include <oboe/Oboe.h>
#include <mutex>

class AudioFormat {
  protected:
    int32_t sampleRate_ = 48000;
    int32_t channelCount_ = 2;
    oboe::AudioFormat format_ = oboe::AudioFormat::I16;

    AudioFormat(int32_t sampleRate, int32_t channelCount,
                oboe::AudioFormat format) :
      sampleRate_(sampleRate), channelCount_(channelCount),
      format_(format) {};
    virtual ~AudioFormat() {}
};

/**
 * An Audio Mixer that mixing input audio stream with a background
 * music. Only works with:
 *   - One background stream
 *   - I16, 48000Hz, dual channel
 *   - raw PCM format without headers
 */
class AudioMixer : public AudioFormat {
  public:
    AudioMixer();
    ~AudioMixer();
    void process(int16_t *liveAudio, int32_t channelCount,
                 int32_t numFrames);
    void addStream(std::unique_ptr<int16_t[]>samples, size_t sampleCount,
                   int32_t sampleRate, int32_t channelCount,
                   oboe::AudioFormat format);
    void setBackgroundMixer(float bgMix);
    bool AudioFormatSupported(int32_t sampleRate, int32_t channels,
                              oboe::AudioFormat format) const;
  private:
    std::unique_ptr<int16_t[]> bgAudio_ = nullptr;
    size_t bgAudioSampleCount_ = 0;
    size_t curPosition_ = 0;
    std::atomic_bool busy_;
    float bgMixFactor_ = 0.5f;
    int32_t  fgMixFactorInt_;
    int32_t  bgMixFactorInt_;
};

/**
 * An audio delay effect:
 *   - decay is how strong echo should be
 *   - delay time is how long to hear the first echo
 *
 *   It is a simple mixing:
 *     new sample = newly_recorded_audio * ( 1 - decay ) +
 *                  feedback_audio * decay
 */
class AudioDelay : public AudioFormat {
  public:
    ~AudioDelay();

    explicit AudioDelay(int32_t sampleRate,
                int32_t channelCount,
                oboe::AudioFormat format,
                float delay,
                float decay);
    void process(int16_t *liveAudio, int32_t channelCount,
               int32_t numFrames);

    bool   setDelay(float delayInSec);
    void   setDecay(float delay);
    float  getDelay(void) const;
    float  getDecay(float)const;

  private:
    float delay_ = 0.0f;
    float decay_ = 0.1f;
    void *buffer_ = nullptr;
    size_t bufCapacity_ = 0;
    size_t bufSize_ = 0;
    size_t curPos_ = 0;
    std::mutex lock_;
    int32_t feedbackFactor_;
    int32_t liveAudioFactor_;
    void allocateBuffer(void);

};
#endif  // EFFECT_PROCESSOR_H
