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

#include "ReverseJniEngine.h"
#include "common/OboeDebug.h"

#define TAG "ReverseJniEngine"

ReverseJniEngine::ReverseJniEngine(JNIEnv *env, jobject thiz) {
    // Store the JavaVM and a global reference to the Java object.
    env->GetJavaVM(&mJavaVM);
    mJavaObject = env->NewGlobalRef(thiz);

    // Get the method ID for the onAudioReady callback in the Java class.
    // The signature "([FII)V" means a method that takes a float array and two ints, and returns void.
    jclass javaClass = env->GetObjectClass(mJavaObject);
    mOnAudioReadyId = env->GetMethodID(javaClass, "onAudioReady", "([FII)V");
    env->DeleteLocalRef(javaClass);
}

ReverseJniEngine::~ReverseJniEngine() {
    // It's important to release the global reference when the engine is destroyed.
    JNIEnv *env;
    mJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    env->DeleteGlobalRef(mJavaObject);
}

void ReverseJniEngine::start(int bufferSizeInBursts) {
    LOGI("Starting audio stream.");
    setupAudioStream();
    setBufferSizeInBursts(bufferSizeInBursts);
    if (mAudioStream) {
        mAudioStream->requestStart();
    }
}

void ReverseJniEngine::stop() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "Stopping audio stream.");
    if (mAudioStream) {
        mAudioStream->requestStop();
        closeAudioStream();
    }
}

void ReverseJniEngine::setBufferSizeInBursts(int bufferSizeInBursts) {
    if (mAudioStream) {
        mAudioStream->setBufferSizeInFrames(bufferSizeInBursts * mAudioStream->getFramesPerBurst());
        LOGI("Buffer size set to %d frames.", mAudioStream->getBufferSizeInFrames());
    }
}

void ReverseJniEngine::setupAudioStream() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(oboe::ChannelCount::Mono)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mAudioStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
    }
}

void ReverseJniEngine::closeAudioStream() {
    if (mAudioStream) {
        mAudioStream->close();
    }
}

oboe::DataCallbackResult ReverseJniEngine::onAudioReady(oboe::AudioStream *oboeStream,
                                                        void *audioData,
                                                        int32_t numFrames) {
    JNIEnv *env;
    // Attach the current thread to the JVM to make JNI calls.
    int getEnvStat = mJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (mJavaVM->AttachCurrentThread(&env, nullptr) != 0) {
            LOGE("Failed to attach current thread to JVM");
            return oboe::DataCallbackResult::Stop;
        }
    }

    int xRunCount = mAudioStream->getXRunCount().value();

    // Create a new Java float array to pass the audio data.
    jfloatArray audioDataArray = env->NewFloatArray(numFrames * mAudioStream->getChannelCount());

    // Call the Java method via reverse JNI.
    env->CallVoidMethod(mJavaObject, mOnAudioReadyId, audioDataArray, numFrames, xRunCount);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        LOGE("Exception thrown in Java onAudioReady");
        env->DeleteLocalRef(audioDataArray);
        return oboe::DataCallbackResult::Stop;
    }

    // Copy the data from the Java float array to the Oboe audio buffer.
    jfloat *arrayElements = env->GetFloatArrayElements(audioDataArray, nullptr);
    memcpy(audioData, arrayElements, static_cast<unsigned long>(numFrames) * mAudioStream->getChannelCount() * sizeof(float));
    env->ReleaseFloatArrayElements(audioDataArray, arrayElements, JNI_ABORT);
    env->DeleteLocalRef(audioDataArray);

    // Detach the thread if we attached it.
    if (getEnvStat == JNI_EDETACHED) {
        mJavaVM->DetachCurrentThread();
    }

    return oboe::DataCallbackResult::Continue;
}
