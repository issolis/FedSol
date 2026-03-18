#include "FedAvg/FedAvg.h"

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <immintrin.h>

std::vector<float> FedAvg::fedAvg(
    std::vector<std::vector<float>> &weights,
    std::vector<uint32_t> sizes)
{
    uint32_t numClients = weights.size();

    if (numClients == 0)
        return {};

    uint32_t numWeights = weights[0].size();

    for (const auto &w : weights)
    {
        if (w.size() != numWeights)
            throw std::runtime_error("Inconsistent weight sizes");
    }

    if (sizes.size() != numClients)
        throw std::runtime_error("Sizes mismatch");

    uint32_t totalData = 0;
    for (auto s : sizes)
        totalData += s;

    std::vector<float> factors(numClients);
    for (uint32_t c = 0; c < numClients; ++c)
    {
        factors[c] = static_cast<float>(sizes[c]) / totalData;
    }

    std::vector<float> result(numWeights, 0.0f);

#pragma omp parallel for
    for (int i = 0; i < (int)numWeights; i += 8)
    {
        if (i + 7 < (int)numWeights)
        {
            __m256 sum = _mm256_setzero_ps();

            for (uint32_t c = 0; c < numClients; ++c)
            {
                __m256 w = _mm256_loadu_ps(&weights[c][i]);
                __m256 factor = _mm256_set1_ps(factors[c]);

                __m256 weighted = _mm256_mul_ps(w, factor);
                sum = _mm256_add_ps(sum, weighted);
            }

            _mm256_storeu_ps(&result[i], sum);
        }
        else
        {

            for (uint32_t j = i; j < numWeights; ++j)
            {
                float sum = 0.0f;

                for (uint32_t c = 0; c < numClients; ++c)
                {
                    sum += factors[c] * weights[c][j];
                }

                result[j] = sum;
            }
        }
    }

    return result;
}