/**
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

#include <jni.h>
#include <logging_macros.h>
#include "EchoAudioEngine.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

static EchoAudioEngine *engine = nullptr;
extern "C" {

JNIEXPORT bool JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_create(JNIEnv *env, jclass) {
  if (engine == nullptr) {
    engine = new EchoAudioEngine();
  }

  return (engine != nullptr);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_delete(JNIEnv *env, jclass) {
  delete engine;
  engine = nullptr;
}

JNIEXPORT void JNICALL Java_com_google_sample_oboe_echo_EchoEngine_setEchoOn(
    JNIEnv *env, jclass, jboolean isEchoOn) {
  if (engine == nullptr) {
    LOGE(
        "Engine is null, you must call createEngine before calling this "
        "method");
    return;
  }

  engine->setEchoOn(isEchoOn);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_setRecordingDeviceId(
    JNIEnv *env, jclass, jint deviceId) {
  if (engine == nullptr) {
    LOGE(
        "Engine is null, you must call createEngine before calling this "
        "method");
    return;
  }

  engine->setRecordingDeviceId(deviceId);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_setPlaybackDeviceId(
    JNIEnv *env, jclass, jint deviceId) {
  if (engine == nullptr) {
    LOGE(
        "Engine is null, you must call createEngine before calling this "
        "method");
    return;
  }

  engine->setPlaybackDeviceId(deviceId);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_setStreamFile(
  JNIEnv *env, jclass type, jobject assetMgr, jstring fileName,
  jint channelCount, jint sampleRate) {

  if (engine == nullptr) {
    LOGE("Engine is null, you must call createEngine "
         "before calling this method");
    return;
  }
  const char *jniFileName = env->GetStringUTFChars(fileName, 0);

  AAssetManager *jniAssetManager = AAssetManager_fromJava(env, assetMgr);
  AAsset *sampleAsset = AAssetManager_open(jniAssetManager, jniFileName,
                                           AASSET_MODE_UNKNOWN);
  size_t sampleCount = static_cast<size_t>
                       (AAsset_getLength(sampleAsset)/2);

  // allocate memory to holds the full clip; the memory is released
  // by the AudioMixer object when it is done.
  std::unique_ptr<int16_t[]> samples =
        std::unique_ptr<int16_t[]>(new int16_t[sampleCount]);
  AAsset_read(sampleAsset, samples.get(), sampleCount * 2);

  engine->setBackgroundStream(std::move(samples), sampleCount,
                              sampleRate, channelCount);
  AAsset_close(sampleAsset);

  env->ReleaseStringUTFChars(fileName, jniFileName);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_setMixer(
      JNIEnv *env, jclass type, jfloat progress) {

  if (engine == nullptr) {
    LOGE("Engine is null, you must call createEngine "
         "before calling this method");
    return;
  }
  engine->setBackgroundMixer(progress);
}

JNIEXPORT void JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_setEchoControls(
  JNIEnv *env, jclass type, jfloat delay, jfloat decay) {

  if (engine == nullptr) {
    LOGE("Engine is null, you must call createEngine "
           "before calling this method");
    return;
  }
  engine->setEchoControls(delay, decay);
}


static const int OBOE_API_AAUDIO = 0;
static const int OBOE_API_OPENSL_ES = 1;

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_setAPI(JNIEnv *env, jclass type, jint apiType) {
  if (engine == nullptr) {
    LOGE("Engine is null, you must call createEngine "
           "before calling this method");
    return JNI_FALSE;
  }

  oboe::AudioApi audioApi;
  switch (apiType) {
    case OBOE_API_AAUDIO:
      audioApi  = oboe::AudioApi::AAudio;
      break;
    case OBOE_API_OPENSL_ES:
      audioApi = oboe::AudioApi::OpenSLES;
      break;
    default:
      LOGE("Unknown API selection to setAPI() %d", apiType);
      return JNI_FALSE;
  }

  return static_cast<jboolean>
            (engine->setAudioApi(audioApi) ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL
Java_com_google_sample_oboe_echo_EchoEngine_isAAudioSupported(JNIEnv *env, jclass type) {
  if (engine == nullptr) {
    LOGE("Engine is null, you must call createEngine "
           "before calling this method");
    return JNI_FALSE;
  }
  return static_cast<jboolean >(engine->isAAudioSupported() ? JNI_TRUE : JNI_FALSE);
}

}

