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

#ifndef OBOE_LATENCY_TUNER_
#define OBOE_LATENCY_TUNER_

#include <atomic>
#include <cstdint>
#include "oboe/Definitions.h"
#include "oboe/AudioStream.h"

namespace oboe {

/**
 * This can be used to dynamically tune the latency of an output stream.
 * It adjusts the bufferSize based on the number of underruns.
 * The bufferSize is the portion of the total bufferCapacity that is used to store data.
 *
 * This only affects the latency associated with the first level of buffering that is closest
 * to the application. It does not affect low latency in the HAL, or touch latency in the UI.
 *
 * Call tune() right before returning from your data callback function if using callbacks.
 * Call tune() right before calling write() if using blocking writes.
 *
 * If you want to see the ongoing results of this tuning process then call
 * stream->getBufferSize() periodically.
 *
 */
class LatencyTuner {
public:
    explicit LatencyTuner(AudioStream &stream);

    /**
     * Adjust the bufferSizeInFrames to optimize latency.
     * It will start with a low latency and then raise it if an underrun occurs.
     *
     * Latency tuning is only supported for AAudio.
     *
     * @return OK or negative error, ErrorUnimplemented for OpenSL ES
     */
    Result tune();

    /**
     * This may be called from another thread. Then tune() will call reset(),
     * which will lower the latency to the minimum and then allow it to rise back up
     * if there are glitches.
     *
     * This is typically called in response to a user decision to minimize latency. In other words,
     * call this from a button handler.
     */
    void requestReset();

private:

    /**
     * Drop the latency down to the minimum and then let it rise back up.
     * This is useful if a glitch caused the latency to increase and it hasn't gone back down.
     *
     * This should only be called in the same thread as tune().
     */
    void reset();

    enum class State {
        Idle,
        Active,
        AtMax,
        Unsupported
    } ;

    // arbitrary number of calls to wait before bumping up the latency
    static constexpr int32_t kIdleCount = 8;

    AudioStream           &mStream;
    State                 mState = State::Idle;
    int32_t               mPreviousXRuns = 0;
    int32_t               mIdleCountDown = 0;
    std::atomic<int32_t>  mLatencyTriggerRequests{0}; // TODO user atomic requester from AAudio
    std::atomic<int32_t>  mLatencyTriggerResponses{0};
};

} // namespace oboe

#endif // OBOE_LATENCY_TUNER_
