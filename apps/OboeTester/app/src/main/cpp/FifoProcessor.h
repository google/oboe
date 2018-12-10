/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef NATIVEOBOE_FIFIPROCESSOR_H
#define NATIVEOBOE_FIFIPROCESSOR_H

#include "AudioProcessorBase.h"
#include "fifo/FifoBuffer.h"

class FifoProcessor : public AudioProcessorBase {
public:
    FifoProcessor(int samplesPerFrame, int numFrames, int threshold);

    virtual ~FifoProcessor();

    uint32_t read(float *destination, int framesToRead) {
        return mFifoBuffer.read(destination, framesToRead);
    }

    uint32_t write(const float *source, int framesToWrite) {
        return mFifoBuffer.write(source, framesToWrite);
    }

    uint32_t getThresholdFrames() {
        return mFifoBuffer.getThresholdFrames();
    }

    void setThresholdFrames(uint32_t threshold) {
        return mFifoBuffer.setThresholdFrames(threshold);
    }

    AudioResult onProcess(
            uint64_t framePosition,
            int numFrames)  override {
        float *buffer = output.getFloatBuffer(numFrames);
        return mFifoBuffer.readNow(buffer, numFrames);
    }

    uint32_t getUnderrunCount() const { return mFifoBuffer.getUnderrunCount(); }

private:
    oboe::FifoBuffer  mFifoBuffer;

public:
    AudioOutputPort output;

};


#endif //NATIVEOBOE_FIFIPROCESSOR_H
