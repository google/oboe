#include <jni.h>
#include <string>

#include <oboe/Oboe.h>
#include "../../../../../../src/common/OboeDebug.h"

using namespace oboe;

extern "C"
JNIEXPORT jstring JNICALL
Java_com_google_oboe_test_oboetest_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject instance) {

    jclass activityClazz = env->FindClass("android/app/Activity");
    jclass contextClazz = env->FindClass("android/content/Context");
    jfieldID audioServiceField = env->GetStaticFieldID(contextClazz, "AUDIO_SERVICE", "Ljava/lang/String;");
    jstring audioServiceString = (jstring) env->GetStaticObjectField(contextClazz, audioServiceField);

    jmethodID getSystemServiceMethod = env->GetMethodID(
            activityClazz, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jobject audioManagerObj = env->CallObjectMethod(
            instance, getSystemServiceMethod, audioServiceString);
    jclass audioManagerClazz = env->FindClass("android/media/AudioManager");

    // Get the audio manager properties
    jmethodID getPropertyMethod = env->GetMethodID(
            audioManagerClazz, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
    jfieldID bufferSizeField =
            env->GetStaticFieldID(audioManagerClazz,
                            "PROPERTY_OUTPUT_FRAMES_PER_BUFFER",
                            "Ljava/lang/String;");
    jstring bufferSizeString =
            (jstring) env->GetStaticObjectField(audioManagerClazz, bufferSizeField);

    jstring bufferSize =
            (jstring) env->CallObjectMethod(audioManagerObj, getPropertyMethod, bufferSizeString);

    return bufferSize;
}