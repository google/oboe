/*
 * Copyright 2016 The Android Open Source Project
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

#include <sys/types.h>

#include "OboeDebug.h"
#include "oboe/Oboe.h"

#include "opensles/OboeStreamOpenSLES.h"
#include "aaudio/OboeStreamAAudio.h"

bool OboeStreamBuilder::isAAudioSupported() {
    return OboeStreamAAudio::isSupported();
}

OboeStream *OboeStreamBuilder::build() {
    LOGD("OboeStreamBuilder.build(): mAudioApi %d, mChannelCount = %d, mFramesPerCallback = %d",
         mAudioApi, mChannelCount, mFramesPerCallback);
    OboeStream *stream = nullptr;
    switch(mAudioApi) {
        case API_UNSPECIFIED:
        case API_AAUDIO:
            if (OboeStreamAAudio::isSupported()) {
                stream = new OboeStreamAAudio(*this);
                break;
            }
            // fall into using older existing API
        case API_OPENSL_ES:
            stream = new OboeStreamOpenSLES(*this);
            break;
    }
    return stream;
}

oboe_result_t OboeStreamBuilder::openStream(OboeStream **streamPP) {
    if (streamPP == nullptr) {
        return OBOE_ERROR_NULL;
    }
    *streamPP = nullptr;
    OboeStream *streamP = build();
    if (streamP == nullptr) {
        return OBOE_ERROR_NULL;
    }
    oboe_result_t result = streamP->open(); // TODO review API
    if (result == OBOE_OK) {
        *streamPP = streamP;
    }
    return result;
}
