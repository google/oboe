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

#include "OboeStreamCallbackProxy.h"

OboeStreamCallbackProxy::~OboeStreamCallbackProxy() {
}

oboe::DataCallbackResult OboeStreamCallbackProxy::onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int numFrames) {
    mCallbackCount++;
    if (mCallbackReturnStop) {
        return oboe::DataCallbackResult::Stop;
    }
    if (mCallback != nullptr) {
        return mCallback->onAudioReady(audioStream, audioData, numFrames);
    }
    return oboe::DataCallbackResult::Stop;
}

void OboeStreamCallbackProxy::onErrorBeforeClose(oboe::AudioStream *audioStream, oboe::Result error) {
    if (mCallback != nullptr) {
        return mCallback->onErrorBeforeClose(audioStream, error);
    }
}

void OboeStreamCallbackProxy::onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result  error) {
    if (mCallback != nullptr) {
        return mCallback->onErrorAfterClose(audioStream, error);
    }
}
