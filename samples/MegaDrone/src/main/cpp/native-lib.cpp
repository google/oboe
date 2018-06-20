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

#include "AudioEngine.h"

AudioEngine engine;

extern "C"
JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_startEngine(JNIEnv *env, jobject instance) {

    engine.start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_stopEngine(JNIEnv *env, jobject instance) {

    engine.stop();

}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_oboe_megadrone_MainActivity_tap(JNIEnv *env, jobject instance, jboolean b) {

    engine.tap(b);
}