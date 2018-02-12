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
#include "OboeStreamBuffered.h"

/**
 * A stream that wraps OpenSL ES.
 *
 * Do not instantiate this class directly.
 * Use an OboeStreamBuilder to create one.
 */
//
class OboeStreamOpenSLES : public OboeStreamBuffered {
public:

    OboeStreamOpenSLES();
    explicit OboeStreamOpenSLES(const OboeStreamBuilder &builder);

    virtual ~OboeStreamOpenSLES();

    oboe_result_t open() override;
    oboe_result_t close() override;

    oboe_result_t requestStart() override;
    oboe_result_t requestPause() override;
    oboe_result_t requestFlush() override;
    oboe_result_t requestStop() override;

    // public, but don't call directly (called by the OSLES callback)
    SLresult enqueueBuffer();

    oboe_result_t waitForStateChange(oboe_stream_state_t currentState,
                                             oboe_stream_state_t *nextState,
                                             int64_t timeoutNanoseconds) override;

    /**
     * Query the current state, eg. OBOE_STREAM_STATE_PAUSING
     *
     * @return state or a negative error.
     */
    oboe_stream_state_t getState() override { return mState; }

    int32_t getFramesPerBurst() override;

protected:
private:

    /**
     * Internal use only.
     * Use this instead of directly setting the internal state variable.
     */
    void setState(oboe_stream_state_t state) {
        mState = state;
    }

    /**
     * Set OpenSL ES state.
     *
     * @param newState SL_PLAYSTATE_PAUSED, SL_PLAYSTATE_PLAYING, SL_PLAYSTATE_STOPPED
     * @return
     */
    oboe_result_t setPlayState(SLuint32 newState);

    uint8_t              *mCallbackBuffer;
    int32_t               mBytesPerCallback;
    int32_t               mFramesPerBurst = 0;
    int32_t               mBurstsPerBuffer = 2; // Double buffered
    oboe_stream_state_t   mState = OBOE_STREAM_STATE_UNINITIALIZED;

    // OpenSLES stuff
    SLObjectItf                   bqPlayerObject_;
    SLPlayItf                     bqPlayerPlay_;
    SLAndroidSimpleBufferQueueItf bq_;
};


#endif // AUDIO_STREAM_OPENSL_ES_H_
