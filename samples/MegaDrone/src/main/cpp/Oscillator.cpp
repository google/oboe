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

#include "Oscillator.h"

// We need to calculate the amplitude value differently for each supported output format
template<>
void Oscillator<float>::setAmplitude(float amplitude){
    mAmplitude = amplitude;
};

template<>
void Oscillator<int16_t>::setAmplitude(float amplitude){
    mAmplitude = amplitude * INT16_MAX;
};