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

#include "aaudio/StreamAAudio.h"
#include "OboeDebug.h"
#include "oboe/Oboe.h"
#include "oboe/StreamBuilder.h"
#include "opensles/StreamOpenSLES.h"

namespace oboe {

bool StreamBuilder::isAAudioSupported() {
    return StreamAAudio::isSupported();
}

Stream *StreamBuilder::build() {
    LOGD("StreamBuilder.build(): mAudioApi %d, mChannelCount = %d, mFramesPerCallback = %d",
         mAudioApi, mChannelCount, mFramesPerCallback);
    Stream *stream = nullptr;
    switch(mAudioApi) {
        case AudioApi::Unspecified:
        case AudioApi::AAudio:
            if (StreamAAudio::isSupported()) {
                stream = new StreamAAudio(*this);
                break;
            }
            // fall into using older existing API
        case AudioApi::OpenSLES:
            stream = new StreamOpenSLES(*this);
            break;
    }
    return stream;
}

Result StreamBuilder::openStream(Stream **streamPP) {
    if (streamPP == nullptr) {
        return Result::ErrorNull;
    }
    *streamPP = nullptr;
    Stream *streamP = build();
    if (streamP == nullptr) {
        return Result::ErrorNull;
    }
    Result result = streamP->open(); // TODO review API
    if (result == Result::OK) {
        *streamPP = streamP;
    }
    return result;
}

} // namespace oboe