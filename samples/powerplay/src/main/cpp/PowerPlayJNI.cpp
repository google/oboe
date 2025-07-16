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


#ifdef __cplusplus

extern "C" {
#endif

using namespace iolib;
using namespace parselib;
using namespace oboe;

static PowerPlayMultiPlayer sDTPlayer;

/**
 * @brief Converts a Kotlin PerformanceMode enum object (passed via JNI) to its corresponding oboe::PerformanceMode.
 *
 * This function retrieves the ordinal value from the Kotlin `PerformanceMode` enum object
 * and maps it to the equivalent `oboe::PerformanceMode` C++ enum value. This is
 * essential for translating performance mode settings from Kotlin/Java layer to the
 * native Oboe audio library for stream configuration.
 *
 * The mapping assumes the following correspondence between the Kotlin enum's ordinal
 * values and the Oboe enum values:
 * - Kotlin `PerformanceMode.None.ordinal` (0) maps to `oboe::PerformanceMode::None`
 * - Kotlin `PerformanceMode.LowLatency.ordinal` (1) maps to `oboe::PerformanceMode::LowLatency`
 * - Kotlin `PerformanceMode.PowerSaving.ordinal` (2) maps to `oboe::PerformanceMode::PowerSaving`
 * - Kotlin `PerformanceMode.PowerSavingOffloaded.ordinal` (3) maps to `oboe::PerformanceMode::POWER_SAVING_OFFLOADED`
 *
 * If the provided `performanceModeObj` is null, if the Java class or its "ordinal" method
 * cannot be found, or if the retrieved ordinal value is outside the expected range [0-3],
 * this function defaults to `oboe::PerformanceMode::None`.
 *
 * @param env Pointer to the JNI environment.
 * @param performanceModeObj A `jobject` representing the Kotlin `PerformanceMode` enum instance.
 * @return The corresponding `oboe::PerformanceMode` enum value, or `oboe::PerformanceMode::None`
 *         in case of errors or an invalid ordinal.
 */
oboe::PerformanceMode getPerformanceMode(JNIEnv *env, jobject performanceModeObj) {
    if (performanceModeObj == nullptr) {
        return PerformanceMode::None; // Default or error case
    }

    jclass performanceModeClass = env->GetObjectClass(performanceModeObj);
    if (performanceModeClass == nullptr) {
        return PerformanceMode::None; // Error finding class
    }

    jmethodID ordinalMethod = env->GetMethodID(performanceModeClass, "ordinal", "()I");
    if (ordinalMethod == nullptr) {
        return PerformanceMode::None; // Error finding method
    }

    jint ordinal = env->CallIntMethod(performanceModeObj, ordinalMethod);

    // Assuming the order of enums in Kotlin matches Oboe's integer representation
    // OboePerformanceMode.NONE.ordinal (0) -> oboe::PerformanceMode::None (10)
    // OboePerformanceMode.LOW_LATENCY.ordinal (1) -> oboe::PerformanceMode::LowLatency (11)
    // OboePerformanceMode.POWER_SAVING.ordinal (2) -> oboe::PerformanceMode::PowerSaving (12)
    // OboePerformanceMode.POWER_SAVING_OFFLOADED.ordinal (3) -> oboe::PerformanceMode::POWER_SAVING_OFFLOADED (13)
    if (ordinal >= 0 && ordinal <= 3) {
        return static_cast<oboe::PerformanceMode>(ordinal + 10);
    } else {
        return PerformanceMode::None; // Default for unknown ordinal
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
