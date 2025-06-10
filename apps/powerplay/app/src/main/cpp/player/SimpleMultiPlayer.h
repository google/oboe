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

#ifndef _PLAYER_SIMPLEMULTIPLAYER_H_
#define _PLAYER_SIMPLEMULTIPLAYER_H_

#include <vector>

#include <oboe/Oboe.h>

#include "OneShotSampleSource.h"
#include "SampleBuffer.h"

namespace iolib {

/**
 * A simple streaming player for multiple SampleBuffers.
 */
    class SimpleMultiPlayer {
    public:
        SimpleMultiPlayer();

        void setupAudioStream(int32_t channelCount, oboe::PerformanceMode performanceMode);

        void teardownAudioStream();

        bool openStream(oboe::PerformanceMode performanceMode);

        bool startStream();

        int getSampleRate() { return mSampleRate; }

        // Wave Sample Loading...
        /**
         * Adds the SampleSource/SampleBuffer pair to the list of source channels.
         * Transfers ownership of those objects so that they can be deleted/unloaded.
         * The indexes associated with each source channel is the order in which they
         * are added.
         */
        void addSampleSource(SampleSource *source, SampleBuffer *buffer);

        /**
         * Deallocates and deletes all added source/buffer (see addSampleSource()).
         */
        void unloadSampleData();

        void triggerDown(int32_t index, oboe::PerformanceMode performanceMode);

        void triggerUp(int32_t index);

        void resetAll();

        bool getOutputReset() { return mOutputReset; }

        void clearOutputReset() { mOutputReset = false; }

        void setPan(int index, float pan);

        float getPan(int index);

        void setGain(int index, float gain);

        float getGain(int index);

        void setLoopMode(int index, bool isLoopMode);

    private:
        class MyDataCallback : public oboe::AudioStreamDataCallback {
        public:
            explicit MyDataCallback(SimpleMultiPlayer *parent) : mParent(parent) {}

            oboe::DataCallbackResult onAudioReady(
                    oboe::AudioStream *audioStream,
                    void *audioData,
                    int32_t numFrames) override;

        private:
            SimpleMultiPlayer *mParent;
        };

        class MyErrorCallback : public oboe::AudioStreamErrorCallback {
        public:
            explicit MyErrorCallback(SimpleMultiPlayer *parent) : mParent(parent) {}

            ~MyErrorCallback() override = default;

            void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override;

        private:
            SimpleMultiPlayer *mParent;
        };

        class MyPresentationCallback : public oboe::AudioStreamPresentationCallback {
        public:
            explicit MyPresentationCallback(SimpleMultiPlayer *parent) : mParent(parent) {}

            ~MyPresentationCallback() override = default;

            void onPresentationEnded(oboe::AudioStream *oboeStream) override;

        private:
            [[maybe_unused]] SimpleMultiPlayer *mParent;
        };

        // Oboe Audio Stream
        std::shared_ptr<oboe::AudioStream> mAudioStream;

        // Playback Audio attributes
        int32_t mChannelCount;
        int32_t mSampleRate;

        // Sample Data
        int32_t mNumSampleBuffers;
        std::vector<SampleBuffer *> mSampleBuffers;
        std::vector<SampleSource *> mSampleSources;

        bool mOutputReset;

        std::shared_ptr<MyDataCallback> mDataCallback;
        std::shared_ptr<MyErrorCallback> mErrorCallback;
        std::shared_ptr<MyPresentationCallback> mPresentationCallback;
    };

}
#endif //_PLAYER_SIMPLEMULTIPLAYER_H_
