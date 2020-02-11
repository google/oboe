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

#include <jni.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <android/log.h>

#include <io/stream/FileInputStream.h>
#include <io/wav/WavStreamReader.h>

#include <player/OneShotSampleBuffer.h>
#include <player/SimpleMultiPlayer.h>

static const char* TAG = "DrumPlayerJNI";

// JNI functions are "C" calling convention
#ifdef __cplusplus
extern "C" {
#endif

using namespace wavlib;

static SimpleMultiPlayer sDTPlayer;

/**
 * Native (JNI) implementation of DrumPlayer.setupAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_setupAudioStreamNative(
        JNIEnv* env, jobject, jint numSampleBuffers, jint numChannels, jint sampleRate) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "init()");

    // we know in this case that the sample buffers are all 1-channel, 41K
    sDTPlayer.setupAudioStream(numSampleBuffers, numChannels, sampleRate);
}

/**
 * Native (JNI) implementation of DrumPlayer.teardownAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_teardownAudioStreamNative(
        JNIEnv* env, jobject, jint numSampleBuffers, jint numChannels, jint sampleRate) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "deinit()");

    // we know in this case that the sample buffers are all 1-channel, 44.1K
    sDTPlayer.teardownAudioStream();
}

/**
 * Native (JNI) implementation of DrumPlayer.loadWavAssetNative()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_loadWavAssetNative(JNIEnv* env, jobject, jbyteArray bytearray, jint index) {
    int len = env->GetArrayLength (bytearray);

    unsigned char* buf = new unsigned char[len];
    env->GetByteArrayRegion (bytearray, 0, len, reinterpret_cast<jbyte*>(buf));
    sDTPlayer.loadSampleDataFromAsset(buf, len, index);
    delete[] buf;
}

/**
 * Native (JNI) implementation of DrumPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_trigger(JNIEnv* env, jobject, jint index) {
    sDTPlayer.triggerDown(index);
}

/**
 * Native (JNI) implementation of DrumPlayer.getOutputReset()
 */
JNIEXPORT jboolean JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_getOutputReset() {
    return sDTPlayer.getOutputReset();
}

/**
 * Native (JNI) implementation of DrumPlayer.clearOutputReset()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_clearOutputReset() {
    sDTPlayer.clearOutputReset();
}

/**
 * Native (JNI) implementation of DrumPlayer.restartStream()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_DrumPlayer_restartStream() {
    sDTPlayer.resetAll();
    sDTPlayer.openStream();
}

#ifdef __cplusplus
}
#endif
