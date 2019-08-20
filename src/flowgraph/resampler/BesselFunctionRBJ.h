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

#ifndef RESAMPLER_BESSEL_FUNCTION_H
#define RESAMPLER_BESSEL_FUNCTION_H

#include <memory>

/**
 * Approximate a zero order Bessel function of the first kind.
 *
 * Based on a posting by Robert Bristow-Johnson at:
 * https://dsp.stackexchange.com/questions/37714/kaiser-window-approximation
 */
class BesselFunctionRBJ {
public:
    /**
     * Higher K gives higher precision.
     * @param K number of terms in the polynomial expansion
     */
    BesselFunctionRBJ(int K = 8)
    : mK(K) {
        mCoefficients = std::make_unique<double[]>(K + 1);
        mCoefficients[0] = 1.0;
        for (int64_t k = 1; k <= K; k++) {
            mCoefficients[k] = mCoefficients[k - 1] * -0.25 / (k * k);
        }
    }

    // Use Horner's method to calculate the polynomial.
    // It is faster than direct() but needs a higher K to converge
    // because the coefficients lose precision.
    double horner(double x) {
        double x2 = x * x;
        double y = x2 * mCoefficients[mK];
        for (int k = mK - 1; k >= 0; k--) {
            y = mCoefficients[k] + x2 * y;
        }
        return y;
    }

    // Direct summation of the first K terms.
    double direct(double x, int K) {
        double z = x * x * -0.25;
        double sum = 0.0;
        for (int k = K; k >= 1; k--) {
            int n = k;
            double term = 1.0;
            do {
                term *= z;
                term /= (n * n);
                n--;
            } while (n >= 1);
            sum += term;
        }
        return sum + 1.0;
    }

private:
    std::unique_ptr<double[]> mCoefficients;
    int mK;
};

#endif //RESAMPLER_BESSEL_FUNCTION_H
