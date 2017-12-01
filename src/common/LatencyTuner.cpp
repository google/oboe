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

#include "oboe/LatencyTuner.h"

using namespace oboe;

LatencyTuner::LatencyTuner(AudioStream &stream)
    : mStream(stream) {
    reset();
}

Result LatencyTuner::tune() {
    if (mState == State::Unsupported) {
        return Result::ErrorUnimplemented;
    }

    Result result = Result::OK;

    // Process reset requests.
    int32_t numRequests = mLatencyTriggerRequests.load();
    if (numRequests != mLatencyTriggerResponses.load()) {
        mLatencyTriggerResponses.store(numRequests);
        reset();
    }

    switch (mState) {
        case State::Idle:
            if (--mIdleCountDown <= 0) {
                mState = State::Active;
            }
            mPreviousXRuns = mStream.getXRunCount();
            if (mPreviousXRuns < 0) {
                result = static_cast<Result>(mPreviousXRuns); // error code
                mState = State::Unsupported;
            }
            break;

        case State::Active: {
            int32_t xRuns = mStream.getXRunCount();
            if ((xRuns - mPreviousXRuns) > 0) {
                mPreviousXRuns = xRuns;
                int32_t oldBufferSize = mStream.getBufferSizeInFrames();
                int32_t requestedBufferSize = oldBufferSize + mStream.getFramesPerBurst();
                int32_t resultingSize = static_cast<int32_t>(
                        mStream.setBufferSizeInFrames(requestedBufferSize));
                if (resultingSize == oldBufferSize) {
                    mState = State::AtMax; // can't go any higher
                } else if (resultingSize < 0) {
                    result = static_cast<Result>(resultingSize); // error code
                    mState = State::Unsupported;
                }
            }
        }

        case State::AtMax:
        case State::Unsupported:
            break;
    }
    return result;
}

void LatencyTuner::requestReset() {
    if (mState != State::Unsupported) {
        mLatencyTriggerRequests++;
    }
}

void LatencyTuner::reset() {
    mState = State::Idle;
    mIdleCountDown = kIdleCount;
    // Set to minimal latency
    mStream.setBufferSizeInFrames(mStream.getFramesPerBurst());
}
