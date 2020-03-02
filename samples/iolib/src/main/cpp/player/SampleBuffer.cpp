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

#include "io/wav/WavStreamReader.h"

using namespace parselib;

namespace iolib {

void SampleBuffer::loadSampleData(WavStreamReader* reader) {
    mAudioProperties.channelCount = reader->getNumChannels();
    mAudioProperties.sampleRate = reader->getSampleRate();

    reader->positionToAudio();

    mNumSampleFrames = reader->getNumSampleFrames() * reader->getNumChannels();
    mSampleData = new float[mNumSampleFrames];
    reader->getDataFloat(mSampleData, reader->getNumSampleFrames());
}

void SampleBuffer::unloadSampleData() {
    delete[] mSampleData;
    mSampleData = nullptr;
    mNumSampleFrames = 0;
}

}
