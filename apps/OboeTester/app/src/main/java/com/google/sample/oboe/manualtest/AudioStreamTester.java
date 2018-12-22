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

package com.google.sample.oboe.manualtest;

import java.io.IOException;

class AudioStreamTester {
    protected AudioStreamBase mCurrentAudioStream;

    AudioStreamBase getCurrentAudioStream() {
        return mCurrentAudioStream;
    }

    public void open(StreamConfiguration requestedConfiguration,
                    StreamConfiguration actualConfiguration) throws IOException {
        mCurrentAudioStream.open(requestedConfiguration, actualConfiguration,
                -1);
    }

    public void start() throws IOException {
        mCurrentAudioStream.start();
    }

    public void stop() throws IOException  {
        mCurrentAudioStream.stop();
    }

    public void pause() throws IOException {
        mCurrentAudioStream.pause();
    }

    public void close() {
        mCurrentAudioStream.close();
    }

    public void startPlayback() throws IOException {
        mCurrentAudioStream.startPlayback();
    }

}
