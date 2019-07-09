/*
 * Copyright 2018 The Android Open Source Project
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
#include <string>
#include <vector>

#include "PlayAudioEngine.h"

PlayAudioEngine *engine; // This should be passed back as a long

std::vector<int> convertJavaArrayToVector(JNIEnv *env, jintArray intArray){

    std::vector<int> v;
    jsize length = env->GetArrayLength(intArray);

    if (length > 0) {
        jboolean isCopy;
        jint *elements = env->GetIntArrayElements(intArray, &isCopy);
        for (int i = 0; i < length; i++) {
            v.push_back(elements[i]);
        }
    }
    return v;
}
extern "C" {
/**
 * Start the audio engine
 *
 * @param env
 * @param instance
 * @param jCpuIds - CPU core IDs which the audio process should affine to
 * @return a pointer to the audio engine. This should be passed to other methods
 */
JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_startEngine(JNIEnv *env, jobject /*unused*/,
                                                         jintArray jCpuIds) {
    std::vector<int> cpuIds = convertJavaArrayToVector(env, jCpuIds);
    engine = new PlayAudioEngine(cpuIds);
}

JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_stopEngine(JNIEnv *env, jobject instance) {
    delete engine;
}


JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_tap(JNIEnv *env, jobject instance, jboolean b) {
    LOGD("Beginning Tap tone");
    if (!engine) {
        LOGE("Engine has not been created");
    }
    engine->toggleTone();
}

JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_native_1setDefaultStreamValues(JNIEnv *env,
                                                                            jclass type,
                                                                            jint sampleRate,
                                                                            jint framesPerBurst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) sampleRate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) framesPerBurst;
}

} // extern "C"