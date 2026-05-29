#include "security/backdoorDefense.h"
#include "logger/logger.h"
#include <algorithm>
#include <cmath>
double BackdoorDefense::norm(const std::vector<float> &v)
{
    double acc = 0.0;
    for (float x : v) acc += static_cast<double>(x) * static_cast<double>(x);
    return std::sqrt(acc);
}
float BackdoorDefense::median(std::vector<float> values)
{
    if (values.empty()) return 0.0f;
    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if (values.size() % 2 == 0)
        return 0.5f * (values[mid - 1] + values[mid]);
    return values[mid];
}
BackdoorDefense::Result BackdoorDefense::filter(
    bool enabled,
    const std::vector<float> &globalWeightsPrev,
    const std::vector<float> &globalWeightsPrevPrev,
    std::vector<std::vector<float>> &weightsList,
    std::vector<uint32_t> &sampleSizesList,
    std::vector<uint32_t> &clientIDs)
{
    Result result;
    result.clientsBefore = weightsList.size();
    result.clientsAfter  = weightsList.size();
    result.acceptedIDs   = clientIDs;
    if (!enabled)
    {
        Logger::log(LogLevel::INFO,
            "[NormClipping] Disabled. Passing all " +
            std::to_string(weightsList.size()) + " client(s) through.");
        return result;
    }
    result.applied = true;
    const bool haveRef = !globalWeightsPrev.empty() &&
                         globalWeightsPrev.size() == weightsList[0].size();
    if (!haveRef)
    {
        Logger::log(LogLevel::WARNING,
            "[NormClipping] No reference model yet. Skipping clip this round.");
        result.skippedNoHistory = true;
        return result;
    }
    const size_t numClients = weightsList.size();
    const size_t dim        = globalWeightsPrev.size();
    std::vector<float> updateNorms(numClients);
    for (size_t c = 0; c < numClients; ++c)
    {
        std::vector<float> delta(dim);
        for (size_t i = 0; i < dim; ++i)
            delta[i] = weightsList[c][i] - globalWeightsPrev[i];
        updateNorms[c] = static_cast<float>(norm(delta));
    }
    const float clipNorm = median(updateNorms);
    Logger::log(LogLevel::INFO,
        "[NormClipping] clip_norm=" + std::to_string(clipNorm));
    for (size_t c = 0; c < numClients; ++c)
    {
        if (updateNorms[c] > clipNorm && clipNorm > 1e-12f)
        {
            const float scale = clipNorm / updateNorms[c];
            for (size_t i = 0; i < dim; ++i)
                weightsList[c][i] = globalWeightsPrev[i] +
                    (weightsList[c][i] - globalWeightsPrev[i]) * scale;
            Logger::log(LogLevel::INFO,
                "[NormClipping] Client " + std::to_string(clientIDs[c]) +
                " clipped: norm " + std::to_string(updateNorms[c]) +
                " -> " + std::to_string(clipNorm));
        }
    }
    return result;
}