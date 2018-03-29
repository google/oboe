#include <jni.h>
#include <android/asset_manager_jni.h>
#include <android/input.h>
#include <utils/logging.h>
#include <memory>

#include "Game.h"

extern "C" {

std::shared_ptr<Game> game;

JNIEXPORT void JNICALL
Java_com_donturner_rhythmgame_MainActivity_native_1onCreate(JNIEnv *env, jobject instance,
                                                            jobject jAssetManager) {

    AAssetManager *assetManager = AAssetManager_fromJava(env, jAssetManager);
    game = std::make_shared<Game>(assetManager);
    game->start();
}

JNIEXPORT void JNICALL
Java_com_donturner_rhythmgame_RendererWrapper_native_1onSurfaceCreated(JNIEnv *env, jobject instance) {
    game->onSurfaceCreated();
}

JNIEXPORT void JNICALL
Java_com_donturner_rhythmgame_RendererWrapper_native_1onSurfaceChanged(JNIEnv *env, jclass type,
                                                                   jint width, jint height) {
    game->onSurfaceChanged(width, height);
}

JNIEXPORT void JNICALL
Java_com_donturner_rhythmgame_RendererWrapper_native_1onDrawFrame(JNIEnv *env, jclass type) {
    game->tick();
}

JNIEXPORT void JNICALL
Java_com_donturner_rhythmgame_GameSurfaceView_native_1onTouchInput(JNIEnv *env, jclass type,
                                                           jint event_type,
                                                           jlong time_since_boot_ms,
                                                           jint pixel_x, jint pixel_y) {
    game->tap(time_since_boot_ms);
}

JNIEXPORT void JNICALL
Java_com_donturner_rhythmgame_GameSurfaceView_native_1surfaceDestroyed__(JNIEnv *env, jclass type) {
    game->onSurfaceDestroyed();
}

}