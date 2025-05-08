/*
 * Copyright 2025 The Android Open Source Project
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

// parselib includes
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>

#include <player/OneShotSampleSource.h>
#include <player/SimpleMultiPlayer.h>

static const char *TAG = "PowerPlayJNI";

// JNI functions are "C" calling convention
#ifdef __cplusplus
extern "C" {
#endif

using namespace iolib;
using namespace parselib;
using namespace oboe;

static SimpleMultiPlayer sDTPlayer;

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.setupAudioStreamNative()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_setupAudioStreamNative(
        JNIEnv *env,
        jobject,
        jint channels
) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "setupAudioStreamNative()");

    // TODO - Dynamically Set Performance Mode
    sDTPlayer.setupAudioStream(channels, oboe::PerformanceMode::None);
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.startAudioStreamNative()
 */
JNIEXPORT jint JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_startAudioStreamNative(
        JNIEnv *,
        jobject
) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "startAudioStreamNative()");
    return (jint) sDTPlayer.startStream();
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.teardownAudioStreamNative()
 */
JNIEXPORT jint JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_teardownAudioStreamNative(
        JNIEnv *,
        jobject
) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "teardownAudioStreamNative()");
    sDTPlayer.teardownAudioStream();

    //TODO - Actually handle a return here.
    return true;
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.loadAssetNative()
 */
JNIEXPORT void JNICALL Java_com_example_powerplay_engine_PowerPlayAudioPlayer_loadAssetNative(
        JNIEnv *env,
        jobject,
        jbyteArray bytearray,
        jint index
) {
    const int32_t len = env->GetArrayLength(bytearray);
    auto *buf = new unsigned char[len];

    env->GetByteArrayRegion(bytearray, 0, len, reinterpret_cast<jbyte *>(buf));

    MemInputStream stream(buf, len);
    WavStreamReader reader(&stream);
    reader.parse();
    reader.getNumChannels();

    auto *sampleBuffer = new SampleBuffer();
    sampleBuffer->loadSampleData(&reader);

    const auto source = new OneShotSampleSource(sampleBuffer, 0);
    sDTPlayer.addSampleSource(source, sampleBuffer);

    delete[] buf;
}

/**
 * Native (JNI) implementation of DrumPlayer.unloadWavAssetsNative()
 */
JNIEXPORT void JNICALL Java_com_example_powerplay_engine_PowerPlayAudioPlayer_unloadAssetsNative(
        JNIEnv *env,
        jobject
) {
    sDTPlayer.unloadSampleData();
}

/**
 * Native (JNI) implementation of DrumPlayer.getOutputReset()
 */
JNIEXPORT jboolean JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_getOutputResetNative(
        JNIEnv *,
        jobject
) {
    return sDTPlayer.getOutputReset();
}

/**
 * Native (JNI) implementation of DrumPlayer.clearOutputReset()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_clearOutputResetNative(
        JNIEnv *,
        jobject
) {
    sDTPlayer.clearOutputReset();
}

/**
 * Native (JNI) implementation of DrumPlayer.trigger()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_startPlayingNative(JNIEnv *env, jobject,
                                                                          jint index,
                                                                          jint offload) {
    auto performanceMode =
            offload == 0 ? PerformanceMode::None
                         : offload == 1 ? PerformanceMode::LowLatency
                         : offload == 2 ? PerformanceMode::PowerSaving
                                        : PerformanceMode::POWER_SAVING_OFFLOADED;
    sDTPlayer.triggerDown(index, performanceMode);
}

/**
 * Native (JNI) implementation of DrumPlayer.trigger()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_stopPlayingNative(JNIEnv *env, jobject,
                                                                         jint index) {
    sDTPlayer.triggerUp(index);
}

/**
 * Native (JNI) implementation of DrumPlayer.trigger()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_setLoopingNative(JNIEnv *env, jobject,
                                                                        jint index,
                                                                        jboolean looping) {
    sDTPlayer.setLoopMode(index, looping);
}

#ifdef __cplusplus
}
#endif
