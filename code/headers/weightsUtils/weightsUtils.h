#ifndef WEIGHT_UTILS_H
#define WEIGHT_UTILS_H

#include <vector>
#include <cstddef>
#include "models/Architecture.h"
#include <utility>

class WeightUtils
{
public:
    static std::pair<size_t, bool>  computeExpectedWeights(const Architecture& arch);
    static bool validateWeights(const Architecture& arch, const std::vector<float>& weights);
    static std::vector<float> generateRandomWeights(size_t size);
};

#endif