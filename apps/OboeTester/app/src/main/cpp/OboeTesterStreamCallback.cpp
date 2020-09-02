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


#include "AudioStreamGateway.h"
#include "oboe/Oboe.h"
#include "common/OboeDebug.h"
#include <sched.h>
#include <cstring>
#include "OboeTesterStreamCallback.h"

// Print if scheduler changes.
void OboeTesterStreamCallback::printScheduler() {
#if OBOE_ENABLE_LOGGING
    int scheduler = sched_getscheduler(gettid());
    if (scheduler != mPreviousScheduler) {
        int schedulerType = scheduler & 0xFFFF; // mask off high flags
        LOGD("callback CPU scheduler = 0x%08x = %s",
             scheduler,
             ((schedulerType == SCHED_FIFO) ? "SCHED_FIFO" :
              ((schedulerType == SCHED_OTHER) ? "SCHED_OTHER" :
               ((schedulerType == SCHED_RR) ? "SCHED_RR" : "UNKNOWN")))
        );
        mPreviousScheduler = scheduler;
    }
#endif
}