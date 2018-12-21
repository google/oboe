/*
 * Copyright 2015 The Android Open Source Project
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

#define MODULE_NAME "OboeTester"

#include <cassert>
#include <cstring>
#include <jni.h>
#include <stdint.h>
#include <thread>

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"

#include "NativeAudioContext.h"

static NativeAudioContext engine;

/*********************************************************************************/
/**********************  JNI  Prototypes *****************************************/
/*********************************************************************************/
extern "C" {

// These must match order in strings.xml and in StreamConfiguration.java
#define NATIVE_MODE_UNSPECIFIED  0
#define NATIVE_MODE_OPENSLES     1
#define NATIVE_MODE_AAUDIO       2

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_openNative(JNIEnv *env, jobject,
                                                       jint sampleRate,
                                                       jint channelCount,
                                                       jint format,
                                                       jint sharingMode,
                                                       jint performanceMode,
                                                       jint deviceId,
                                                       jint sessionId,
                                                       jint framesPerBurst,
                                                       jboolean isInput);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_close(JNIEnv *env, jobject);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_start(JNIEnv *env, jobject);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_pause(JNIEnv *env, jobject);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_stop(JNIEnv *env, jobject);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setThresholdInFrames(JNIEnv *env, jobject, jint);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getThresholdInFrames(JNIEnv *env, jobject);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getBufferCapacityInFrames(JNIEnv *env, jobject);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setNativeApi(JNIEnv *env, jobject, jint);

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setUseCallback(JNIEnv *env, jclass type,
                                                                      jboolean useCallback);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setCallbackReturnStop(JNIEnv *env,
                                                                             jclass type,
                                                                             jboolean b);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setCallbackSize(JNIEnv *env, jclass type,
                                                            jint callbackSize);

// ================= OboeAudioOutputStream ================================

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setToneEnabled(JNIEnv *env, jobject, jboolean);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setToneType(JNIEnv *env, jobject, jint);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setAmplitude(JNIEnv *env, jobject, jdouble);

/*********************************************************************************/
/**********************  JNI Implementations *************************************/
/*********************************************************************************/
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_openNative(
        JNIEnv *env, jobject synth,
        jint sampleRate,
        jint channelCount,
        jint format,
        jint sharingMode,
        jint performanceMode,
        jint deviceId,
        jint sessionId,
        jint framesPerBurst,
        jboolean isInput) {
    LOGD("OboeAudioStream_openNative: sampleRate = %d, framesPerBurst = %d", sampleRate, framesPerBurst);

    return (jint) engine.open(sampleRate,
                              channelCount,
                              format,
                              sharingMode,
                              performanceMode,
                              deviceId,
                              sessionId,
                              framesPerBurst,
                              isInput);
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_startNative(JNIEnv *env, jobject) {
    return (jint) engine.start();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_pauseNative(JNIEnv *env, jobject) {
    return (jint) engine.pause();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_stopNative(JNIEnv *env, jobject) {
    return (jint) engine.stop();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_startPlaybackNative(JNIEnv *env, jobject) {
    return (jint) engine.startPlayback();
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_close(JNIEnv *env, jobject) {
    engine.close();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setBufferSizeInFrames(
        JNIEnv *env, jobject, jint threshold) {
    if (engine.oboeStream != nullptr) {
        auto result = engine.oboeStream->setBufferSizeInFrames(threshold);
        return (!result)
               ? (jint) result.error()
               : (jint) result.value();
    }
    return (jint) oboe::Result::ErrorNull;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getBufferSizeInFrames(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getBufferSizeInFrames();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getBufferCapacityInFrames(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getBufferCapacityInFrames();
    }
    return result;
}

static oboe::AudioApi convertNativeApiToAudioApi(int audioApi) {
    switch (audioApi) {
        default:
        case NATIVE_MODE_UNSPECIFIED:
            return oboe::AudioApi::Unspecified;
        case NATIVE_MODE_AAUDIO:
            return oboe::AudioApi::AAudio;
        case NATIVE_MODE_OPENSLES:
            return oboe::AudioApi::OpenSLES;
    }
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setNativeApi(
        JNIEnv *env, jobject, jint nativeApi) {
    jint result = (jint) oboe::Result::OK;
    LOGD("OboeAudioStream_setNativeApi(%d)", nativeApi);
    switch (nativeApi) {
        case NATIVE_MODE_UNSPECIFIED:
        case NATIVE_MODE_AAUDIO:
        case NATIVE_MODE_OPENSLES:
            engine.setAudioApi(convertNativeApiToAudioApi(nativeApi));
            break;
        default:
            result = (jint) oboe::Result::ErrorOutOfRange;
            break;
    }
    return result;
}

static int convertAudioApiToNativeApi(oboe::AudioApi audioApi) {
    switch(audioApi) {
        case oboe::AudioApi::Unspecified:
            return NATIVE_MODE_UNSPECIFIED;
        case oboe::AudioApi::OpenSLES:
            return NATIVE_MODE_OPENSLES;
        case oboe::AudioApi::AAudio:
            return NATIVE_MODE_AAUDIO;
        default:
            return -1;
    }
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getNativeApi(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        oboe::AudioApi audioApi = engine.oboeStream->getAudioApi();
        result = convertAudioApiToNativeApi(audioApi);
        LOGD("OboeAudioStream_getNativeApi got %d", result);
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getSampleRate(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getSampleRate();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getSharingMode(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = (jint) engine.oboeStream->getSharingMode();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getPerformanceMode(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = (jint) engine.oboeStream->getPerformanceMode();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFramesPerBurst(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getFramesPerBurst();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getChannelCount(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getChannelCount();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFormat(JNIEnv *env, jobject instance) {
        jint result = (jint) oboe::Result::ErrorNull;
        if (engine.oboeStream != nullptr) {
            result = (jint) engine.oboeStream->getFormat();
        }
        return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getDeviceId(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getDeviceId();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getSessionId(
        JNIEnv *env, jobject) {
    jint result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getSessionId();
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFramesWritten(
        JNIEnv *env, jobject) {
    jlong result = (jint) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getFramesWritten();
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFramesRead(
        JNIEnv *env, jobject) {
    jlong result = (jlong) oboe::Result::ErrorNull;
    if (engine.oboeStream != nullptr) {
        result = engine.oboeStream->getFramesRead();
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getCallbackCount(
        JNIEnv *env, jobject) {
    return engine.getCallbackCount();
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getLatency(JNIEnv *env, jobject instance) {
    if (engine.oboeStream != nullptr) {
        auto result = engine.oboeStream->calculateLatencyMillis();
        return (!result) ? -1.0 : result.value();
    }
    return -1.0;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getState(JNIEnv *env, jobject instance) {
    if (engine.oboeStream != nullptr) {
        auto state = engine.oboeStream->getState();
        if (state != oboe::StreamState::Starting && state != oboe::StreamState::Started) {
            oboe::Result result = engine.oboeStream->waitForStateChange(
                    oboe::StreamState::Uninitialized,
                    &state, 0);
            if (result != oboe::Result::OK) state = oboe::StreamState::Unknown;
        }
        return (jint) state;
    }
    return -1;
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_AudioInputTester_getPeakLevel(JNIEnv *env,
                                                          jobject instance,
                                                          jint index) {
    return engine.mInputAnalyzer.getPeakLevel(index);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setUseCallback(JNIEnv *env, jclass type,
                                                                      jboolean useCallback) {
    engine.useCallback = useCallback;
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setCallbackReturnStop(JNIEnv *env, jclass type,
                                                                      jboolean b) {
    engine.setCallbackReturnStop(b);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setCallbackSize(JNIEnv *env, jclass type,
                                                            jint callbackSize) {
    engine.callbackSize = callbackSize;
}

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_isMMap(JNIEnv *env, jobject instance) {
    return engine.mIsMMapUsed;
}

// ================= OboeAudioOutputStream ================================

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setToneEnabled(
        JNIEnv *env, jobject, jboolean enabled) {
    engine.setToneEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setToneType(
        JNIEnv *env, jobject, jint toneType) {
    engine.setToneType(toneType);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setAmplitude(
        JNIEnv *env, jobject, jdouble amplitude) {
    engine.setAmplitude(amplitude);

}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setChannelEnabled(
        JNIEnv *env, jobject, jint channelIndex, jboolean enabled) {
    engine.setChannelEnabled(channelIndex, enabled);
}

}
