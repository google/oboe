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

#ifndef NATIVEOBOE_OBOESTREAMCALLBACKPROXY_H
#define NATIVEOBOE_OBOESTREAMCALLBACKPROXY_H

#include <unistd.h>
#include <sys/types.h>

#include "oboe/Oboe.h"

class OboeStreamCallbackProxy : public oboe::AudioStreamCallback {
public:
    OboeStreamCallbackProxy() {}
    ~OboeStreamCallbackProxy();

    void setCallback(oboe::AudioStreamCallback *callback) {
        mCallback = callback;
        setCallbackCount(0);
    }

    void setCallbackReturnStop(bool b) {
        mCallbackReturnStop = b;
    }

    int64_t getCallbackCount() {
        return mCallbackCount;
    }

    void setCallbackCount(int64_t count) {
        mCallbackCount = count;
    }

    /**
     * Called when the stream is ready to process audio.
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int numFrames) override;

    void onErrorBeforeClose(oboe::AudioStream *audioStream, oboe::Result error) override;

    void onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result error) override;

private:
    oboe::AudioStreamCallback *mCallback = nullptr;

    bool                       mCallbackReturnStop = false;
    int64_t                    mCallbackCount = 0;
};


#endif //NATIVEOBOE_OBOESTREAMCALLBACKPROXY_H
