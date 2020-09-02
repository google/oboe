/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef OBOETESTER_STREAM_CALLBACK_H
#define OBOETESTER_STREAM_CALLBACK_H

#include <unistd.h>
#include <sys/types.h>
#include "flowgraph/FlowGraphNode.h"
#include "oboe/Oboe.h"

class OboeTesterStreamCallback : public oboe::AudioStreamCallback {
public:
    virtual ~OboeTesterStreamCallback() = default;

    // Call this before starting.
    void reset() {
        mPreviousScheduler = -1;
    }

protected:
    void    printScheduler();

    int     mPreviousScheduler = -1;
};


#endif //OBOETESTER_STREAM_CALLBACK_H
