/*
 * Copyright 2022 The Android Open Source Project
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

#ifndef OBOETESTER_TEST_ERROR_CALLBACK_H
#define OBOETESTER_TEST_ERROR_CALLBACK_H

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"
#include <mutex>
#include <thread>
#include <vector>

/**
 * This code is an experiment to see if we can cause a crash from the ErrorCallback.
 *
 * Starting from Android 17, a stream on the legacy path may be rerouted instead of
 * disconnected when the routed device changes. In that case the error callback is
 * never called, so the same experiment is also performed from the routing callback.
 */
class TestErrorCallback {
public:

    oboe::Result open();
    oboe::Result start();
    oboe::Result stop();
    oboe::Result close();

    int test();

    int32_t getCallbackMagic() {
        return mCallbackMagic.load();
    }

    /**
     * @return the last unexpected (non-disconnect) error reported by the error callback,
     *         or 0 if none.
     */
    int32_t getCallbackError() {
        return mCallbackError.load();
    }

protected:

    std::atomic<int32_t> mCallbackMagic{0};
    std::atomic<int32_t> mCallbackError{0};

private:

    void cleanup() {
        // The error callback and the routing callback may both fire for the same
        // routing change and each will call cleanup() from its own thread.
        // Serialize them so we do not reset the same shared_ptr concurrently.
        std::lock_guard<std::mutex> lock(mCleanupLock);
        mDataCallback.reset();
        mErrorCallback.reset();
        mRoutingCallback.reset();
        mStream.reset();
    }

    class MyDataCallback : public oboe::AudioStreamDataCallback {    public:

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

    };

    class MyErrorCallback : public oboe::AudioStreamErrorCallback {
    public:

        MyErrorCallback(TestErrorCallback *parent): mParent(parent) {}

        virtual ~MyErrorCallback() {
            // If the delete occurs before onErrorAfterClose() then this bad magic
            // value will be seen by the Java test code, causing a failure.
            // It is also possible that this code will just cause OboeTester to crash!
            mMagic = 0xdeadbeef;
            LOGE("%s() called", __func__);
        }

        void onErrorBeforeClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
            LOGE("%s() - error = %s, parent = %p",
                 __func__, oboe::convertToText(error), &mParent);
            // Trigger a crash by "deleting" this callback object while in use!
            // Do not try this at home. We are just trying to reproduce the crash
            // reported in #1603.
            std::thread t([this]() {
                    this->mParent->cleanup(); // Possibly delete stream and callback objects.
                    LOGE("onErrorBeforeClose called cleanup!");
                });
            t.detach();
            // There is a race condition between the deleting thread and this thread.
            // We do not want to add synchronization because the object is getting deleted
            // and cannot be relied on.
            // So we sleep here to give the deleting thread a chance to win the race.
            usleep(10 * 1000);
        }

        void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
            // The callback was probably deleted by now.
            LOGE("%s() - error = %s, mMagic = 0x%08X",
                 __func__, oboe::convertToText(error), mMagic.load());
            if (error == oboe::Result::ErrorDisconnected) {
                mParent->mCallbackMagic = mMagic.load();
            } else {
                // Report an unexpected error separately so the UI can show a
                // meaningful message instead of a bogus "magic" value.
                mParent->mCallbackError = static_cast<int32_t>(error);
            }
        }

    private:
        TestErrorCallback *mParent;
        // This must match the value in TestErrorCallbackActivity.java
        static constexpr int32_t kMagicGood = 0x600DCAFE;
        std::atomic<int32_t> mMagic{kMagicGood};
    };

    class MyRoutingCallback : public oboe::AudioStreamRoutingCallback {
    public:

        MyRoutingCallback(TestErrorCallback *parent): mParent(parent) {}

        virtual ~MyRoutingCallback() {
            // If the delete occurs before onRoutingChanged() finishes then this bad magic
            // value will be seen by the Java test code, causing a failure.
            mMagic = 0xdeadbeef;
            LOGE("%s() called", __func__);
        }

        void onRoutingChanged(oboe::AudioStream *oboeStream,
                              const int32_t *deviceIds,
                              int32_t numDevices) override {
            LOGE("%s() - parent = %p, numDevices = %d", __func__, &mParent, numDevices);
            // The framework may report the initial routing of the stream shortly after it
            // starts. That is not a device change, so ignore it and wait for a real
            // plug/unplug event.
            std::vector<int32_t> newDeviceIds(deviceIds, deviceIds + numDevices);
            if (newDeviceIds == mParent->mInitialDeviceIds) {
                LOGE("%s() - routing unchanged from the initial devices, ignoring", __func__);
                return;
            }
            // Trigger a crash by "deleting" this callback object while in use!
            // This mirrors what MyErrorCallback::onErrorBeforeClose() does.
            std::thread t([this]() {
                    this->mParent->cleanup(); // Possibly delete stream and callback objects.
                    LOGE("onRoutingChanged called cleanup!");
                });
            t.detach();
            // Sleep here to give the deleting thread a chance to win the race.
            usleep(10 * 1000);
            oboeStream->close();
            // The callback was probably deleted by now.
            LOGE("%s() - mMagic = 0x%08X", __func__, mMagic.load());
            mParent->mCallbackMagic = mMagic.load();
        }

    private:
        TestErrorCallback *mParent;
        // This must match the value in TestErrorCallbackActivity.java
        static constexpr int32_t kMagicGood = 0x600DCAFE;
        std::atomic<int32_t> mMagic{kMagicGood};
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;
    std::shared_ptr<MyErrorCallback> mErrorCallback;
    std::shared_ptr<MyRoutingCallback> mRoutingCallback;
    std::mutex mCleanupLock;
    // Devices the stream was routed to when it was opened. Only written in open(),
    // before any callback can fire.
    std::vector<int32_t> mInitialDeviceIds;

    static constexpr int kChannelCount = 2;
};

#endif //OBOETESTER_TEST_ERROR_CALLBACK_H
