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

#ifndef OBOETESTER_FULL_DUPLEX_GLITCHES_H
#define OBOETESTER_FULL_DUPLEX_GLITCHES_H

#include <unistd.h>
#include <sys/types.h>

#include "oboe/Oboe.h"
#include "FullDuplexAnalyzer.h"
#include "analyzer/GlitchAnalyzer.h"

class FullDuplexGlitches : public FullDuplexAnalyzer {
public:
    FullDuplexGlitches() {
        setMNumInputBurstsCushion(1);
    }

    bool isDone() {
        return false;
    }

    GlitchAnalyzer *getGlitchAnalyzer() {
        return &mGlitchAnalyzer;
    }

    LoopbackProcessor *getLoopbackProcessor() override {
        return (LoopbackProcessor *) &mGlitchAnalyzer;
    }

private:

    GlitchAnalyzer  mGlitchAnalyzer;

};


#endif //OBOETESTER_FULL_DUPLEX_GLITCHES_H

