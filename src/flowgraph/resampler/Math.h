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

#ifndef RESAMPLER_MATH_H
#define RESAMPLER_MATH_H

class Math {
public:
static double bessel(double x) {
    double y = cosh(0.970941817426052 * x);
    y += cosh(0.8854560256532099 * x);
    y += cosh(0.7485107481711011 * x);
    y += cosh(0.5680647467311558 * x);
    y += cosh(0.3546048870425356 * x);
    y += cosh(0.120536680255323 * x);
    y *= 2;
    y += cosh(x);
    y /= 13;
    return y;
}

};
#endif //RESAMPLER_MATH_H
