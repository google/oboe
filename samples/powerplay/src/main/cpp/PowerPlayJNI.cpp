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
#include "PowerPlayMultiPlayer.h"

static const char *TAG = "PowerPlayJNI";

#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

using namespace iolib;
using namespace parselib;
using namespace oboe;

// Global static player instance.
// For more complex scenarios, consider passing this as a native peer (long) from Java.
static PowerPlayMultiPlayer sDTPlayer;

/**
 * @brief Converts a Kotlin PerformanceMode enum object (passed via JNI) to its corresponding oboe::PerformanceMode.
 *
 * (Documentation remains the same as your improved version, it's good)
 */
oboe::PerformanceMode getPerformanceMode(JNIEnv *env, jobject performanceModeObj) {
    if (performanceModeObj == nullptr) {
        LOG_ERROR("performanceModeObj is null in getPerformanceMode");
        return PerformanceMode::None;
    }

    jclass performanceModeClass = env->GetObjectClass(performanceModeObj);
    if (performanceModeClass == nullptr) {
        LOG_ERROR("Failed to get class for performanceModeObj");
        // An exception might be pending here if GetObjectClass failed.
        if (env->ExceptionCheck()) {
            env->ExceptionClear(); // Clear it if we are returning a default.
        }
        return PerformanceMode::None;
    }

    jmethodID ordinalMethod = env->GetMethodID(performanceModeClass, "ordinal", "()I");
    // It's good practice to delete local refs when done, though JNI cleans them on native method return.
    env->DeleteLocalRef(performanceModeClass); // Delete local ref

    if (ordinalMethod == nullptr) {
        LOG_ERROR("Failed to get 'ordinal' method ID");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return PerformanceMode::None;
    }

    jint ordinal = env->CallIntMethod(performanceModeObj, ordinalMethod);
    if (env->ExceptionCheck()) {
        LOG_ERROR("Exception occurred calling 'ordinal' method.");
        env->ExceptionClear();
        return PerformanceMode::None;
    }

    // Mapping based on Kotlin enum ordinals to Oboe PerformanceMode values
    switch (ordinal) {
        case 0:
            return PerformanceMode::None;
        case 1:
            return PerformanceMode::LowLatency;
        case 2:
            return PerformanceMode::PowerSaving;
        case 3:
            return PerformanceMode::POWER_SAVING_OFFLOADED;
        default:
            LOG_ERROR("Unknown performance mode ordinal: %d", ordinal);
            return PerformanceMode::None;
    }
}

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
 * Native (JNI) implementation of PowerPlayAudioEngine.unloadWavAssetsNative()
 */
JNIEXPORT void JNICALL Java_com_example_powerplay_engine_PowerPlayAudioPlayer_unloadAssetsNative(
        JNIEnv *env,
        jobject
) {
    sDTPlayer.unloadSampleData();
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.getOutputReset()
 */
JNIEXPORT jboolean JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_getOutputResetNative(
        JNIEnv *,
        jobject
) {
    return sDTPlayer.getOutputReset();
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.clearOutputReset()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_clearOutputResetNative(
        JNIEnv *,
        jobject
) {
    sDTPlayer.clearOutputReset();
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.trigger()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_startPlayingNative(JNIEnv *env, jobject,
                                                                          jint index,
                                                                          jobject mode) {
    auto performanceMode = getPerformanceMode(env, mode);
    sDTPlayer.triggerDown(index, performanceMode);
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.trigger()
 */
JNIEXPORT void JNICALL
Java_com_example_powerplay_engine_PowerPlayAudioPlayer_stopPlayingNative(JNIEnv *env, jobject,
                                                                         jint index) {
    sDTPlayer.triggerUp(index);
}

/**
 * Native (JNI) implementation of PowerPlayAudioEngine.trigger()
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
