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

#include "Synth.h"
#include "Constants.h"
#include "MonoToStereo.h"

void Synth::start() {

    AudioStreamBuilder builder;
    builder.setCallback(this);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);

    builder.openStream(&mStream);

    const float baseOscFrequency = 116.0;
    const float divisor = 33;
    const float amplitude = 0.009;

    if (mStream->getFormat() == AudioFormat::Float){

        mMixerFloat = std::make_unique<Mixer<float>>();

        // Spin up the oscillators (float versions)
        for (int i = 0; i < kNumOscillators; ++i) {
            mOscsFloat[i].setSampleRate(mStream->getSampleRate());
            mOscsFloat[i].setFrequency(baseOscFrequency+((float)i/divisor));
            mOscsFloat[i].setAmplitude(amplitude);
            mMixerFloat->addTrack(&mOscsFloat[i]);
        }

        if (mStream->getChannelCount() == kStereoChannelCount){
            mOutputStageFloat = std::make_shared<MonoToStereo<float>>(mMixerFloat.get());
        } else if (mStream->getChannelCount() == kMonoChannelCount){
            mOutputStageFloat = mMixerFloat;
        }

    } else {

        mMixerInt16 = std::make_unique<Mixer<int16_t>>();

        // Spin up the oscillators (16-bit int versions)
        for (int i = 0; i < kNumOscillators; ++i) {
            mOscsInt16[i].setSampleRate(mStream->getSampleRate());
            mOscsInt16[i].setFrequency(baseOscFrequency+((float)i/divisor));
            mOscsInt16[i].setAmplitude((int16_t)(amplitude * INT16_MAX));
            mMixerInt16->addTrack(&mOscsInt16[i]);
        }

        if (mStream->getChannelCount() == kStereoChannelCount){
            mOutputStageInt16 = std::make_shared<MonoToStereo<int16_t>>(mMixerInt16.get());
        } else if (mStream->getChannelCount() == kMonoChannelCount){
            mOutputStageInt16 = mMixerInt16;
        }
    }

    mStream->setBufferSizeInFrames(mStream->getFramesPerBurst() * 2);
    mStream->requestStart();
}

void Synth::tap(bool isOn) {

    if (mStream){
        if (mStream->getFormat() == AudioFormat::Float){
            for (auto &osc : mOscsFloat) osc.setWaveOn(isOn);
        } else {
            for (auto &osc : mOscsInt16) osc.setWaveOn(isOn);
        }
    }
}

DataCallbackResult
Synth::onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {

    if (mStream->getFormat() == AudioFormat::Float){
        mOutputStageFloat->renderAudio(static_cast<float *>(audioData), numFrames);
    } else {
        mOutputStageInt16->renderAudio(static_cast<int16_t *>(audioData), numFrames);
    }

    return DataCallbackResult::Continue;
}
