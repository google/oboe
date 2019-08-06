/*
 * Copyright 2018 The Android Open Source Project
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


#include <memory>
#include "MegaDroneEngine.h"

MegaDroneEngine::MegaDroneEngine(std::vector<int> cpuIds) : AudioEngine(std::make_shared<DefaultAudioStreamCallback>()) {
    mDefaultCallbackPtr->setParent(*this);
    mAudioSource =  std::make_shared<Synth>(mStream->getSampleRate(), mStream->getChannelCount());
    mDefaultCallbackPtr->setSource(std::dynamic_pointer_cast<IRenderableAudio>(mAudioSource));
    startPlaybackStream();
     mCpuIds = std::move(cpuIds);
    if (!mIsThreadAffinitySet) {
        setThreadAffinity();
        mIsThreadAffinitySet = true;
    }
}

void MegaDroneEngine::tap(bool isDown) {
    mAudioSource->tap(isDown);
}

/**
 * Set the thread affinity for the current thread to mCpuIds. This can be useful to call on the
 * audio thread to avoid underruns caused by CPU core migrations to slower CPU cores.
 */
void MegaDroneEngine::setThreadAffinity() {

    pid_t current_thread_id = gettid();
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);

    // If the callback cpu ids aren't specified then bind to the current cpu
    if (mCpuIds.empty()) {
        int current_cpu_id = sched_getcpu();
        LOGV("Current CPU ID is %d", current_cpu_id);
        CPU_SET(current_cpu_id, &cpu_set);
    } else {

        for (size_t i = 0; i < mCpuIds.size(); i++) {
            int cpu_id = mCpuIds.at(i);
            LOGV("CPU ID %d added to cores set", cpu_id);
            CPU_SET(cpu_id, &cpu_set);
        }
    }

    int result = sched_setaffinity(current_thread_id, sizeof(cpu_set_t), &cpu_set);
    if (result == 0) {
        LOGV("Thread affinity set");
    } else {
        LOGW("Error setting thread affinity. Error no: %d", result);
    }

    mIsThreadAffinitySet = true;
}
