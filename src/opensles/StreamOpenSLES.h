/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef AUDIO_STREAM_OPENSL_ES_H_
#define AUDIO_STREAM_OPENSL_ES_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "oboe/Oboe.h"
#include "StreamBuffered.h"

namespace oboe {

/**
 * A stream that wraps OpenSL ES.
 *
 * Do not instantiate this class directly.
 * Use an OboeStreamBuilder to create one.
 */
//
class StreamOpenSLES : public StreamBuffered {
public:

    StreamOpenSLES();
    explicit StreamOpenSLES(const StreamBuilder &builder);

    virtual ~StreamOpenSLES();

    Result open() override;
    Result close() override;

    Result requestStart() override;
    Result requestPause() override;
    Result requestFlush() override;
    Result requestStop() override;

    // public, but don't call directly (called by the OSLES callback)
    SLresult enqueueBuffer();

    Result waitForStateChange(StreamState currentState,
                                             StreamState *nextState,
                                             int64_t timeoutNanoseconds) override;

    /**
     * Query the current state, eg. OBOE_STREAM_STATE_PAUSING
     *
     * @return state or a negative error.
     */
    StreamState getState() override { return mState; }

    int32_t getFramesPerBurst() override;

protected:
private:

    /**
     * Internal use only.
     * Use this instead of directly setting the internal state variable.
     */
    void setState(StreamState state) {
        mState = state;
    }

    /**
     * Set OpenSL ES state.
     *
     * @param newState SL_PLAYSTATE_PAUSED, SL_PLAYSTATE_PLAYING, SL_PLAYSTATE_STOPPED
     * @return
     */
    Result setPlayState(SLuint32 newState);

    uint8_t              *mCallbackBuffer;
    int32_t               mBytesPerCallback;
    int32_t               mFramesPerBurst = 0;
    int32_t               mBurstsPerBuffer = 2; // Double buffered
    StreamState           mState = StreamState::Uninitialized;

    // OpenSLES stuff
    SLObjectItf                   bqPlayerObject_;
    SLPlayItf                     bqPlayerPlay_;
    SLAndroidSimpleBufferQueueItf bq_;
};

} // namespace oboe

#endif // AUDIO_STREAM_OPENSL_ES_H_
