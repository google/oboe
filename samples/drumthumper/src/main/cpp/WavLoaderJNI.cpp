/*
 * Copyright 2019 The Android Open Source Project
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

#include <io/stream/FileInputStream.h>
#include <io/wav/WavStreamReader.h>

static const char* TAG = "WavLoaderJNI";

#ifdef __cplusplus
extern "C" {
#endif

using namespace wavlib;

JNIEXPORT void JNICALL Java_com_google_oboe_sample_drumthumper_WavLoader_loadWavFile(JNIEnv* env, jobject, jstring filePath) {
    const char *nativeFilePath = env->GetStringUTFChars(filePath, 0);

    __android_log_print(ANDROID_LOG_INFO, TAG, "nativeFilePath:%s", nativeFilePath);

    int fh = open(nativeFilePath, O_RDONLY);
    __android_log_print(ANDROID_LOG_INFO, TAG, "fh:%d", fh);

    env->ReleaseStringUTFChars(filePath, nativeFilePath);

    FileInputStream* stream = new FileInputStream(fh);
    WavStreamReader* reader = new WavStreamReader(stream);
    reader->parse();


    close(fh);
}

#ifdef __cplusplus
}
#endif

