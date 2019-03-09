/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef OBOETESTER_FULL_DUPLEX_ECHO_H
#define OBOETESTER_FULL_DUPLEX_ECHO_H

#include <unistd.h>
#include <sys/types.h>

#include "oboe/Oboe.h"
#include "FullDuplexStream.h"

class FullDuplexEcho : public FullDuplexStream {
public:
    FullDuplexEcho() {}

    /**
     * Called when data is available on both streams.
     * Caller should override this method.
     */
    oboe::DataCallbackResult onBothStreamsReady(
            const void *inputData,
            int   numInputFrames,
            void *outputData,
            int   numOutputFrames
    ) override;

    oboe::Result start() override;

private:
    std::unique_ptr<float[]> mDelayLine;
    int32_t mCursorFrames = 0;
    int32_t mDelaySizeFrames = 0;
};


#endif //OBOETESTER_FULL_DUPLEX_ECHO_H
