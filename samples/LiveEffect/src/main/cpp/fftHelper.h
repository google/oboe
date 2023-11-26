//
// Created by jakub on 20.11.2023.
//

#ifndef SAMPLES_FFTHELPER_H
#define SAMPLES_FFTHELPER_H

#include <complex>

//const double PI = 3.1415926536;
//
//// using hardcoded data type float
//
//class FftHelper {
//private:
//    unsigned int _bitReverse(unsigned int x, int log2n) {
//        int n = 0;
////        int mask = 0x1;
//        for (int i=0; i < log2n; i++) {
//            n <<= 1;
//            n |= (x & 1);
//            x >>= 1;
//        }
//        return n;
//    }
//
//    const std::complex<float> J;
//public:
//    FftHelper(): J(0, 1) {};
//    ~FftHelper() {};
//
//    void calculateFft(float * inputArray, float * outputArray, int log2n) {
//        int n = 1 << log2n;
//        for (unsigned int i=0; i < n; ++i) {
//            b[bitReverse(i, log2n)] = a[i];
//        }
//        for (int s = 1; s <= log2n; ++s) {
//            int m = 1 << s;
//            int m2 = m >> 1;
//            this->complex w(1, 0);
//            complex wm = exp(-J * (PI / m2));
//            for (int j=0; j < m2; ++j) {
//                for (int k=j; k < n; k += m) {
//                    complex t = w * b[k + m2];
//                    complex u = b[k];
//                    b[k] = u + t;
//                    b[k + m2] = u - t;
//                }
//                w *= wm;
//            }
//        }
//    };
//};

#endif //SAMPLES_FFTHELPER_H
