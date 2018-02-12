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
#include "AudioStreamBuffered.h"

namespace oboe {

class OpenSLEngine {
public:
    static OpenSLEngine *getInstance();

    SLresult open();

    void close();

    SLresult createOutputMix(SLObjectItf *objectItf);

    SLresult createAudioPlayer(SLObjectItf *objectItf,
                               SLDataSource *audioSource,
                               SLDataSink *audioSink);

private:
    static OpenSLEngine *sInstance;

// engine interfaces
    int32_t sOpenCount = 0;
    SLObjectItf sEngineObject = 0;
    SLEngineItf sEngineEngine;
};

class OpenSLOutputMixer {
public:
    static OpenSLOutputMixer *getInstance();

    SLresult open();

    void close();

    SLresult createAudioPlayer(SLObjectItf *objectItf,
                               SLDataSource *audioSource);

private:
    static OpenSLOutputMixer *sInstance;

// engine interfaces
    int32_t sOpenCount = 0;
// output mix interfaces
    SLObjectItf sOutputMixObject = 0;
};

/**
 * A stream that wraps OpenSL ES.
 *
 * Do not instantiate this class directly.
 * Use an OboeStreamBuilder to create one.
 */
//
class AudioStreamOpenSLES : public AudioStreamBuffered {
public:

    AudioStreamOpenSLES();
    explicit AudioStreamOpenSLES(const AudioStreamBuilder &builder);

    virtual ~AudioStreamOpenSLES();

    virtual Result open() override;
    virtual Result close() override;

    // public, but don't call directly (called by the OSLES callback)
    SLresult enqueueBuffer();

    /**
     * Query the current state, eg. OBOE_STREAM_STATE_PAUSING
     *
     * @return state or a negative error.
     */
    StreamState getState() override { return mState; }

    int32_t getFramesPerBurst() override;

    static SLuint32 getDefaultByteOrder();

    virtual int chanCountToChanMask(int chanCount) = 0;

protected:
    /**
     * Internal use only.
     * Use this instead of directly setting the internal state variable.
     */
    void setState(StreamState state) {
        mState = state;
    }

    uint8_t              *mCallbackBuffer;
    int32_t               mBytesPerCallback;
    int32_t               mFramesPerBurst = 0;
    int32_t               mBurstsPerBuffer = 2; // Double buffered
    StreamState           mState = StreamState::Uninitialized;

    // OpenSLES stuff
    SLAndroidSimpleBufferQueueItf bq_ = nullptr;
};

} // namespace oboe

#endif // AUDIO_STREAM_OPENSL_ES_H_
