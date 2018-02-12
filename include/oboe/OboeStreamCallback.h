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
     * @param oboeStream pointer to the associated stream
     * @param audioData buffer containing input data or a place to put output data
     * @param numFrames number of frames to be processed
     * @return OBOE_CALLBACK_RESULT_CONTINUE or OBOE_CALLBACK_RESULT_STOP
     */
    virtual oboe_data_callback_result_t onAudioReady(
            OboeStream *oboeStream,
            void *audioData,
            int32_t numFrames) = 0;

    /**
     * This will be called when an error occurs on a stream or when the stream is discomnnected.
     * The underlying stream will already be stopped by Oboe but not yet closed.
     * So the stream can be queried.
     *
     * @param oboeStream pointer to the associated stream
     * @param error
     */
    virtual void onErrorBeforeClose(OboeStream *oboeStream, oboe_result_t error) {}

    /**
     * This will be called when an error occurs on a stream or when the stream is disconnected.
     * The underlying stream will already be stopped AND closed by Oboe.
     * So the underlyng stream cannot be referenced.
     *
     * This callback could be used to reopen a new stream on another device.
     *
     * @param oboeStream pointer to the associated stream
     * @param error
     */
    virtual void onErrorAfterClose(OboeStream *oboeStream, oboe_result_t error) {}

};

#endif //OBOE_OBOE_STREAM_CALLBACK_H
