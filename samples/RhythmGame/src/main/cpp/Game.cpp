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

#include <utils/logging.h>
#include <thread>

#include "Game.h"

Game::Game(AAssetManager *assetManager): mAssetManager(assetManager) {
}

void Game::start() {
    // TODO: Add your code here
}

void Game::tap(int64_t eventTimeAsUptime) {
    // TODO: Add your code here
}

void Game::tick(){
    // TODO: Add your code here
}

void Game::onSurfaceCreated() {
    SetGLScreenColor(kScreenBackgroundColor);
}

void Game::onSurfaceChanged(int widthInPixels, int heightInPixels) {
}

void Game::onSurfaceDestroyed() {
}
