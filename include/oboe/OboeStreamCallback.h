/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OBOE_OBOE_STREAM_CALLBACK_H
#define OBOE_OBOE_STREAM_CALLBACK_H

#include "oboe/OboeDefinitions.h"

class OboeStream;

class OboeStreamCallback {
public:
    virtual ~OboeStreamCallback() = default;
    /**
     * A buffer is ready for processing.
     *
     * @param input buffer containing recorded audio, may be NULL
     * @param output fill this with audio data to be played, may be NULL
     * @param number of frames of input or output
     * @return OBOE_CALLBACK_RESULT_CONTINUE or OBOE_CALLBACK_RESULT_STOP
     */
    virtual oboe_data_callback_result_t onAudioReady(
            OboeStream *audioStream,
            void *audioData,
            int32_t numFrames) = 0;

    virtual void onError(OboeStream *audioStream, oboe_result_t error) {}

};


#endif //OBOE_OBOE_STREAM_CALLBACK_H
