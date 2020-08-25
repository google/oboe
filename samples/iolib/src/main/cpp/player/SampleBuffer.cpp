/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "SampleBuffer.h"

#include "wav/WavStreamReader.h"

namespace iolib {

void SampleBuffer::loadSampleData(parselib::WavStreamReader* reader) {
    // Although we read this in, at this time we know a-priori that the data is mono
    mAudioProperties.channelCount = reader->getNumChannels();
    mAudioProperties.sampleRate = reader->getSampleRate();

    reader->positionToAudio();

    mNumSamples = reader->getNumSampleFrames() * reader->getNumChannels();
    mSampleData = new float[mNumSamples];

    reader->getDataFloat(mSampleData, reader->getNumSampleFrames());
}

void SampleBuffer::unloadSampleData() {
    if (mSampleData != nullptr) {
        delete[] mSampleData;
        mSampleData = nullptr;
    }
    mNumSamples = 0;
}

}
