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

#include "SynthWorkload.h"

void SynthWorkload::onCallback(double workload) {
    // If workload changes then restart notes.
    if (workload != mPreviousWorkload) {
        mSynth.allNotesOff();
        mAreNotesOn = false;
        mCountdown = 0; // trigger notes on
        mPreviousWorkload = workload;
    }
    if (mCountdown <= 0) {
        if (mAreNotesOn) {
            mSynth.allNotesOff();
            mAreNotesOn = false;
            mCountdown = mOffFrames;
        } else {
            mSynth.notesOn((int)mPreviousWorkload);
            mAreNotesOn = true;
            mCountdown = mOnFrames;
        }
    }
}

/**
 * Render the notes into a stereo buffer.
 * Passing a nullptr will cause the calculated results to be discarded.
 * The workload should be the same.
 * @param buffer a real stereo buffer or nullptr
 * @param numFrames
 */
void SynthWorkload::renderStereo(float *buffer, int numFrames) {
    if (buffer == nullptr) {
        int framesLeft = numFrames;
        while (framesLeft > 0) {
            int framesThisTime = std::min(kDummyBufferSizeInFrames, framesLeft);
            // Do the work then throw it away.
            mSynth.renderStereo(mDummyStereoBuffer.get(), framesThisTime);
            framesLeft -= framesThisTime;
        }
    } else {
        mSynth.renderStereo(buffer, numFrames);
    }
    mCountdown -= numFrames;
}

SynthWorkload::SynthWorkload() {
    mSynth.setup(marksynth::kSynthmarkSampleRate, marksynth::kSynthmarkMaxVoices);
}


SynthWorkload::SynthWorkload(int onFrames, int offFrames) {
    mSynth.setup(marksynth::kSynthmarkSampleRate, marksynth::kSynthmarkMaxVoices);
    mOnFrames = onFrames;
    mOffFrames = offFrames;
}
