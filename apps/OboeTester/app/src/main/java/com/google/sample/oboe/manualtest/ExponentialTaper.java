/*
 * Copyright 2017 The Android Open Source Project
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

package com.google.sample.oboe.manualtest;

/**
 * Maps integer range info to a double value along an exponential scale.
 *
 * <pre>
 *
 *   x = ival / mResolution
 *   f(x) = a*(root**bx)
 *   f(0.0) = dmin
 *   f(1.0) = dmax
 *
 *   f(0.0) = a * 1.0 => a = dmin
 *   f(1.0) = dmin * root**b = dmax
 *   b = log(dmax / dmin) / log(root)
 *
 * </pre>
 */

public class ExponentialTaper {
    private int mResolution;
    private double a = 1.0;
    private double b = 2.0;
    private double dmin = 1.0;
    private double dmax = 100.0;
    private double ROOT = 10.0;

    public ExponentialTaper(int res, double dmin, double dmax) {
        this.mResolution = res;
        this.dmin = dmin;
        this.dmax = dmax;
        updateCoefficients();
    }

    private void updateCoefficients() {
        a = dmin;
        b = Math.log10(dmax / dmin);
    }

    public double linearToExponential(int ival) {
        double x = (double) ival / mResolution;
        double y = a * Math.pow(ROOT, b * x);
        return y;
    }
}
