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

NativeAudioContext engine;

/*********************************************************************************/
/**********************  JNI  Prototypes *****************************************/
/*********************************************************************************/
extern "C" {

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_openNative(JNIEnv *env, jobject,
                                                       jint nativeApi,
                                                       jint sampleRate,
                                                       jint channelCount,
                                                       jint format,
                                                       jint sharingMode,
                                                       jint performanceMode,
                                                       jint inputPreset,
                                                       jint deviceId,
                                                       jint sessionId,
                                                       jint framesPerBurst,
                                                       jboolean channelConversionAllowed,
                                                       jboolean formatConversionAllowed,
                                                       jint rateConversionQuality,
                                                       jboolean isMMap,
                                                       jboolean isInput);
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_close(JNIEnv *env, jobject, jint);

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setThresholdInFrames(JNIEnv *env, jobject, jint, jint);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getThresholdInFrames(JNIEnv *env, jobject, jint);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getBufferCapacityInFrames(JNIEnv *env, jobject, jint);
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setNativeApi(JNIEnv *env, jobject, jint, jint);

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

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_manualtest_NativeEngine_isMMapSupported(JNIEnv *env, jclass type) {
    return AAudioExtensions::getInstance().isMMapSupported();
}

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_manualtest_NativeEngine_isMMapExclusiveSupported(JNIEnv *env, jclass type) {
    return AAudioExtensions::getInstance().isMMapExclusiveSupported();
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_NativeEngine_setWorkaroundsEnabled(JNIEnv *env, jclass type,
                                                                          jboolean enabled) {
    oboe::OboeGlobals::setWorkaroundsEnabled(enabled);
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_openNative(
        JNIEnv *env, jobject synth,
        jint nativeApi,
        jint sampleRate,
        jint channelCount,
        jint format,
        jint sharingMode,
        jint performanceMode,
        jint inputPreset,
        jint deviceId,
        jint sessionId,
        jint framesPerBurst,
        jboolean channelConversionAllowed,
        jboolean formatConversionAllowed,
        jint rateConversionQuality,
        jboolean isMMap,
        jboolean isInput) {
    LOGD("OboeAudioStream_openNative: sampleRate = %d, framesPerBurst = %d", sampleRate, framesPerBurst);

    return (jint) engine.getCurrentActivity()->open(nativeApi,
                                                    sampleRate,
                                                    channelCount,
                                                    format,
                                                    sharingMode,
                                                    performanceMode,
                                                    inputPreset,
                                                    deviceId,
                                                    sessionId,
                                                    framesPerBurst,
                                                    channelConversionAllowed,
                                                    formatConversionAllowed,
                                                    rateConversionQuality,
                                                    isMMap,
                                                    isInput);
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_TestAudioActivity_startNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->start();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_TestAudioActivity_pauseNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->pause();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_TestAudioActivity_stopNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->stop();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_TestAudioActivity_getFramesPerCallback(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->getFramesPerCallback();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_startPlaybackNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->startPlayback();
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_close(JNIEnv *env, jobject, jint streamIndex) {
    engine.getCurrentActivity()->close(streamIndex);
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setBufferSizeInFrames(
        JNIEnv *env, jobject, jint streamIndex, jint threshold) {
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto result = oboeStream->setBufferSizeInFrames(threshold);
        return (!result)
               ? (jint) result.error()
               : (jint) result.value();
    }
    return (jint) oboe::Result::ErrorNull;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getBufferSizeInFrames(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getBufferSizeInFrames();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getBufferCapacityInFrames(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getBufferCapacityInFrames();
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
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        oboe::AudioApi audioApi = oboeStream->getAudioApi();
        result = convertAudioApiToNativeApi(audioApi);
        LOGD("OboeAudioStream_getNativeApi got %d", result);
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getSampleRate(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getSampleRate();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getSharingMode(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getSharingMode();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getPerformanceMode(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getPerformanceMode();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getInputPreset(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getInputPreset();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFramesPerBurst(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getFramesPerBurst();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getChannelCount(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getChannelCount();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFormat(JNIEnv *env, jobject instance, jint streamIndex) {
        jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getFormat();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getDeviceId(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getDeviceId();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getSessionId(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getSessionId();
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFramesWritten(
        JNIEnv *env, jobject, jint streamIndex) {
    jlong result = (jint) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getFramesWritten();
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getFramesRead(
        JNIEnv *env, jobject, jint streamIndex) {
    jlong result = (jlong) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getFramesRead();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getXRunCount(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jlong) oboe::Result::ErrorNull;
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto oboeResult  = oboeStream->getXRunCount();
        if (!oboeResult) {
            result = (jint) oboeResult.error();
        } else {
            result = oboeResult.value();
        }
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getCallbackCount(
        JNIEnv *env, jobject) {
    return engine.getCurrentActivity()->getCallbackCount();
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getLatency(JNIEnv *env, jobject instance, jint streamIndex) {
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto result = oboeStream->calculateLatencyMillis();
        return (!result) ? -1.0 : result.value();
    }
    return -1.0;
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getCpuLoad(JNIEnv *env, jobject instance, jint streamIndex) {
    return engine.getCurrentActivity()->getCpuLoad();
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setWorkload(
        JNIEnv *env, jobject, jdouble workload) {
    engine.getCurrentActivity()->setWorkload(workload);
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getState(JNIEnv *env, jobject instance, jint streamIndex) {
    oboe::AudioStream *oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto state = oboeStream->getState();
        if (state != oboe::StreamState::Starting && state != oboe::StreamState::Started) {
            oboe::Result result = oboeStream->waitForStateChange(
                    oboe::StreamState::Uninitialized,
                    &state, 0);

            if (result != oboe::Result::OK){
                if (result == oboe::Result::ErrorClosed) {
                    state = oboe::StreamState::Closed;
                } else if (result == oboe::Result::ErrorDisconnected){
                    state = oboe::StreamState::Disconnected;
                } else {
                    state = oboe::StreamState::Unknown;
                }
            }
        }
        return (jint) state;
    }
    return -1;
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_AudioInputTester_getPeakLevel(JNIEnv *env,
                                                          jobject instance,
                                                          jint index) {
    return engine.getCurrentActivity()->getPeakLevel(index);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setUseCallback(JNIEnv *env, jclass type,
                                                                      jboolean useCallback) {
    ActivityContext::mUseCallback = useCallback;
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setCallbackReturnStop(JNIEnv *env, jclass type,
                                                                      jboolean b) {
    OboeStreamCallbackProxy::setCallbackReturnStop(b);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_setCallbackSize(JNIEnv *env, jclass type,
                                                            jint callbackSize) {
    ActivityContext::callbackSize = callbackSize;
}

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_isMMap(JNIEnv *env, jobject instance, jint streamIndex) {
    return engine.getCurrentActivity()->isMMapUsed(streamIndex);
}

// ================= OboeAudioOutputStream ================================

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setToneEnabled(
        JNIEnv *env, jobject, jboolean enabled) {
    engine.getCurrentActivity()->setEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setToneType(
        JNIEnv *env, jobject, jint toneType) {
// FIXME    engine.getCurrentActivity()->setToneType(toneType);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setChannelEnabled(
        JNIEnv *env, jobject, jint channelIndex, jboolean enabled) {
    engine.getCurrentActivity()->setChannelEnabled(channelIndex, enabled);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioOutputStream_setSignalType(
        JNIEnv *env, jobject, jint signalType) {
    engine.getCurrentActivity()->setSignalType(signalType);
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_OboeAudioStream_getOboeVersionNumber(JNIEnv *env,
                                                                          jclass type) {
    return OBOE_VERSION_NUMBER;
}

// ==========================================================================
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_TestAudioActivity_setActivityType(JNIEnv *env,
                                                                         jobject instance,
                                                                         jint activityType) {
    engine.setActivityType(activityType);
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_TestInputActivity_saveWaveFile(JNIEnv *env,
                                                                        jobject instance,
                                                                        jstring fileName) {
    const char *str = env->GetStringUTFChars(fileName, nullptr);
    LOGD("nativeSaveFile(%s)", str);
    jint result = engine.getCurrentActivity()->saveWaveFile(str);
    env->ReleaseStringUTFChars(fileName, str);
    return result;
}

// ==========================================================================
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_TestInputActivity_setMinimumFramesBeforeRead(JNIEnv *env,
                                                                      jobject instance,
                                                                      jint numFrames) {
    engine.getCurrentActivity()->setMinimumFramesBeforeRead(numFrames);
}

// ==========================================================================
JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_EchoActivity_setDelayTime(JNIEnv *env,
                                                                         jobject instance,
                                                                         jdouble delayTimeSeconds) {
    engine.setDelayTime(delayTimeSeconds);
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_RoundTripLatencyActivity_getAnalyzerProgress(JNIEnv *env,
                                                                                    jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getProgress();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_RoundTripLatencyActivity_getMeasuredLatency(JNIEnv *env,
                                                                                   jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getMeasuredLatency();
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_RoundTripLatencyActivity_getMeasuredConfidence(JNIEnv *env,
                                                                                      jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getMeasuredConfidence();
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_RoundTripLatencyActivity_getBackgroundRMS(JNIEnv *env,
                                                                                 jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getBackgroundRMS();
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_RoundTripLatencyActivity_getSignalRMS(JNIEnv *env,
                                                                                 jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getSignalRMS();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_AnalyzerActivity_getMeasuredResult(JNIEnv *env,
                                                                          jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getResult();
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_AnalyzerActivity_getAnalyzerState(JNIEnv *env,
                                                                         jobject instance) {
    return ((ActivityFullDuplex *)engine.getCurrentActivity())->getState();
}

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_manualtest_AnalyzerActivity_isAnalyzerDone(JNIEnv *env,
                                                                       jobject instance) {
    return ((ActivityFullDuplex *)engine.getCurrentActivity())->isAnalyzerDone();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_AnalyzerActivity_getResetCount(JNIEnv *env,
                                                                          jobject instance) {
    return ((ActivityFullDuplex *)engine.getCurrentActivity())->getResetCount();
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_GlitchActivity_getGlitchCount(JNIEnv *env,
                                                                     jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getGlitchCount();
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_GlitchActivity_getStateFrameCount(JNIEnv *env,
                                                                     jobject instance,
                                                                     jint state) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getStateFrameCount(state);
}

JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_GlitchActivity_getSignalToNoiseDB(JNIEnv *env,
                                                                         jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getSignalToNoiseDB();
}
JNIEXPORT jdouble JNICALL
Java_com_google_sample_oboe_manualtest_GlitchActivity_getPeakAmplitude(JNIEnv *env,
                                                                       jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getPeakAmplitude();
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_manualtest_GlitchActivity_setTolerance(JNIEnv *env,
                                                                   jobject instance,
                                                                   jfloat tolerance) {
    if (engine.mActivityGlitches.getGlitchAnalyzer()) {
        engine.mActivityGlitches.getGlitchAnalyzer()->setTolerance(tolerance);
    }
}

JNIEXPORT jint JNICALL
Java_com_google_sample_oboe_manualtest_ManualGlitchActivity_getGlitch(JNIEnv *env, jobject instance,
                                                                      jfloatArray waveform_) {
    float *waveform = env->GetFloatArrayElements(waveform_, nullptr);
    jsize length = env->GetArrayLength(waveform_);
    jsize numSamples = 0;
    auto *analyzer = engine.mActivityGlitches.getGlitchAnalyzer();
    if (analyzer) {
        numSamples = analyzer->getLastGlitch(waveform, length);
    }

    env->ReleaseFloatArrayElements(waveform_, waveform, 0);
    return numSamples;
}

}
