#ifndef OBOETESTER_AVERAGEBUFFER_H
#define OBOETESTER_AVERAGEBUFFER_H

#include <vector>
#include <algorithm>

class AverageBuffer {
public:
    AverageBuffer() = default;
    
    void resize(size_t size) {
        mAccumulator.resize(size);
        clear();
    }
    
    void clear() {
        std::fill(mAccumulator.begin(), mAccumulator.end(), 0.0);
        mCount = 0;
    }
    
    void accumulate(const double* data, size_t size) {
        if (size > mAccumulator.size()) {
            mAccumulator.resize(size, 0.0);
        }
        for (size_t i = 0; i < size; ++i) {
            mAccumulator[i] += data[i];
        }
        mCount++;
    }

    int getCount() const {
        return mCount;
    }

    size_t size() const {
        return mAccumulator.size();
    }

    double getAverageAt(size_t index) const {
        if (mCount > 0 && index < mAccumulator.size()) {
            return mAccumulator[index] / mCount;
        }
        return 0.0;
    }

private:
    std::vector<double> mAccumulator;
    int                 mCount = 0;
};

#endif //OBOETESTER_AVERAGEBUFFER_H
