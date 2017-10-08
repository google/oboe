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

#include "oboe/OboeLatencyTuner.h"

OboeLatencyTuner::OboeLatencyTuner(OboeStream &stream)
    : mStream(stream) {
    reset();
}

oboe_result_t OboeLatencyTuner::tune() {
    if (mState == STATE_UNSUPPORTED) {
        return OBOE_ERROR_UNIMPLEMENTED;
    }

    oboe_result_t result = OBOE_OK;

    // Process reset requests.
    int32_t numRequests = mLatencyTriggerRequests.load();
    if (numRequests != mLatencyTriggerResponses.load()) {
        mLatencyTriggerResponses.store(numRequests);
        reset();
    }

    switch (mState) {
        case STATE_IDLE:
            if (--mIdleCountDown <= 0) {
                mState = STATE_ACTIVE;
            }
            mPreviousXRuns = mStream.getXRunCount();
            if (mPreviousXRuns < 0) {
                result = mPreviousXRuns; // error code
                mState = STATE_UNSUPPORTED;
            }
            break;

        case STATE_ACTIVE: {
            int32_t xRuns = mStream.getXRunCount();
            if ((xRuns - mPreviousXRuns) > 0) {
                mPreviousXRuns = xRuns;
                int32_t oldBufferSize = mStream.getBufferSizeInFrames();
                int32_t requestedBufferSize = oldBufferSize + mStream.getFramesPerBurst();
                int32_t resultingSize = mStream.setBufferSizeInFrames(requestedBufferSize);
                if (resultingSize == oldBufferSize) {
                    mState = STATE_AT_MAX; // can't go any higher
                } else if (resultingSize < 0) {
                    result = resultingSize; // error code
                    mState = STATE_UNSUPPORTED;
                }
            }
        }

        case STATE_AT_MAX:
        case STATE_UNSUPPORTED:
            break;
    }
    return result;
}

void OboeLatencyTuner::requestReset() {
    if (mState != STATE_UNSUPPORTED) {
        mLatencyTriggerRequests++;
    }
}

void OboeLatencyTuner::reset() {
    mState = STATE_IDLE;
    mIdleCountDown = IDLE_COUNT;
    // Set to minimal latency
    mStream.setBufferSizeInFrames(mStream.getFramesPerBurst());
}
