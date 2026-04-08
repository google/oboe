/*
 * Copyright 2026 The Android Open Source Project
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
#include <memory>
#include "DJEngine.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace oboedj;

// Global instance of the engine
static std::unique_ptr<DJEngine> gEngine;

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_initNative(JNIEnv *env, jobject thiz) {
    if (!gEngine) {
        gEngine = std::make_unique<DJEngine>();
    }
    gEngine->openStream();
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_startNative(JNIEnv *env, jobject thiz) {
    if (gEngine) {
        gEngine->startStream();
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_stopNative(JNIEnv *env, jobject thiz) {
    if (gEngine) {
        gEngine->stopStream();
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_loadTrackNative(JNIEnv *env, jobject thiz, jbyteArray track_bytes, jint deck_index) {
    if (!gEngine) return;

    jsize len = env->GetArrayLength(track_bytes);
    jbyte* buffer = env->GetByteArrayElements(track_bytes, nullptr);

    if (buffer != nullptr) {
        gEngine->loadTrack(reinterpret_cast<uint8_t*>(buffer), len, deck_index);
        env->ReleaseByteArrayElements(track_bytes, buffer, JNI_ABORT); // JNI_ABORT means we don't copy back
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_setSpeedNative(JNIEnv *env, jobject thiz, jint deck_index, jfloat speed) {
    if (gEngine) {
        gEngine->setDeckSpeed(deck_index, speed);
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_setPlayingNative(JNIEnv *env, jobject thiz, jint deck_index, jboolean is_playing) {
    if (gEngine) {
        gEngine->setDeckPlaying(deck_index, is_playing);
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_oboedj_DJEngine_setCrossfaderNative(JNIEnv *env, jobject thiz, jfloat position) {
    if (gEngine) {
        gEngine->setCrossfader(position);
    }
}

#ifdef __cplusplus
}
#endif
