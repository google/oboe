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

class DoubleStatistics {
public:
    void add(double statistic) {
        if (skipCount < kNumberStatisticsToSkip) {
            skipCount++;
        } else {
            if (statistic <= 0.0) return;
            sum = statistic + sum;
            count++;
            minimum = std::min(statistic, minimum.load());
            maximum = std::max(statistic, maximum.load());
        }
    }

    double getAverage() const {
        return sum / count;
    }

    std::string dump() const {
        if (count == 0) return "?";
        char buff[100];
        snprintf(buff, sizeof(buff), "%3.1f/%3.1f/%3.1f ms", minimum.load(), getAverage(), maximum.load());
        std::string buffAsStr = buff;
        return buffAsStr;
    }

    void clear() {
        skipCount = 0;
        sum = 0;
        count = 0;
        minimum = DBL_MAX;
        maximum = 0;
    }

private:
    static constexpr double kNumberStatisticsToSkip = 5; // Skip the first 5 frames
    std::atomic<int> skipCount { 0 };
    std::atomic<double> sum { 0 };
    std::atomic<int> count { 0 };
    std::atomic<double> minimum { DBL_MAX };
    std::atomic<double> maximum { 0 };
};

class OboeStreamCallbackProxy : public oboe::AudioStreamCallback {
public:
    void setCallback(oboe::AudioStreamCallback *callback) {
        mCallback = callback;
        setCallbackCount(0);
        mStatistics.clear();
    }

    static void setCallbackReturnStop(bool b) {
        mCallbackReturnStop = b;
    }

    int64_t getCallbackCount() {
        return mCallbackCount;
    }

    void setCallbackCount(int64_t count) {
        mCallbackCount = count;
    }

    int32_t getFramesPerCallback() {
        return mFramesPerCallback.load();
    }

    /**
     * Called when the stream is ready to process audio.
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int numFrames) override;

    /**
     * Specify the amount of artificial workload that will waste CPU cycles
     * and increase the CPU load.
     * @param workload typically ranges from 0.0 to 100.0
     */
    void setWorkload(double workload) {
        mWorkload = std::max(0.0, workload);
    }

    double getWorkload() const {
        return mWorkload;
    }

    double getCpuLoad() const {
        return mCpuLoad;
    }

    std::string getCallbackTimeString() const {
        return mStatistics.dump();
    }

    static int64_t getNanoseconds(clockid_t clockId = CLOCK_MONOTONIC);

private:
    static constexpr int32_t   kWorkloadScaler = 500;
    static constexpr double    kNsToMsScaler = 0.000001;
    double                     mWorkload = 0.0;
    std::atomic<double>        mCpuLoad{0};
    int64_t                    mPreviousCallbackTimeNs = 0;
    DoubleStatistics           mStatistics;

    oboe::AudioStreamCallback *mCallback = nullptr;
    static bool                mCallbackReturnStop;
    int64_t                    mCallbackCount = 0;
    std::atomic<int32_t>       mFramesPerCallback{0};
};


#endif //NATIVEOBOE_OBOESTREAMCALLBACKPROXY_H
