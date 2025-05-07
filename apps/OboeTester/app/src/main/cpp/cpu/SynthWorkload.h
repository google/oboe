/*
 * Copyright 2025 The Android Open Source Project
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

#ifndef SYNTH_WORKLOAD_H
#define SYNTH_WORKLOAD_H

#include "../synth/Synthesizer.h"

class SynthWorkload {
public:
    SynthWorkload();
    SynthWorkload(int onFrames, int offFrames);
    void onCallback(double workload);
    void renderStereo(float *buffer, int numFrames);

private:
    marksynth::Synthesizer   mSynth;
    static constexpr int     kDummyBufferSizeInFrames = 32;
    float                    mDummyStereoBuffer[kDummyBufferSizeInFrames * 2];
    double                   mPreviousWorkload = 1.0;
    bool                     mAreNotesOn = false;
    int                      mCountdown = 0;
    int                      mOnFrames = 0;
    int                      mOffFrames = 0;
};

#endif //SYNTH_WORKLOAD_H
