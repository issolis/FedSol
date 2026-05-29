#include "security/backdoorDefense.h"
#include "logger/logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
double BackdoorDefense::dot(const std::vector<float> &a, const std::vector<float> &b)
{
    double acc = 0.0;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
        acc += static_cast<double>(a[i]) * static_cast<double>(b[i]);
    return acc;
}
double BackdoorDefense::norm(const std::vector<float> &v)
{
    return std::sqrt(dot(v, v));
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
std::vector<float> BackdoorDefense::subtract(const std::vector<float> &a,
                                              const std::vector<float> &b)
{
    std::vector<float> result(a.size());
    for (size_t i = 0; i < a.size(); ++i)
        result[i] = a[i] - b[i];
    return result;
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
    if (!enabled)
    {
        Logger::log(LogLevel::INFO,
            "[BackdoorDefense] Disabled by config. Passing all " +
            std::to_string(weightsList.size()) + " client(s) through.");
        result.acceptedIDs = clientIDs;
        return result;
    }
    result.applied = true;
    const bool haveHistory =
        !globalWeightsPrev.empty() &&
        !globalWeightsPrevPrev.empty() &&
        globalWeightsPrev.size() == globalWeightsPrevPrev.size() &&
        globalWeightsPrev.size() == weightsList[0].size();
    if (!haveHistory)
    {
        Logger::log(LogLevel::WARNING,
            "[BackdoorDefense] Insufficient global model history (need 2 rounds). "
            "Skipping filter this round — behaving as vanilla FedAvg.");
        result.acceptedIDs    = clientIDs;
        result.skippedNoHistory = true;
        return result;
    }
    const std::vector<float> deltaGlobal = subtract(globalWeightsPrev, globalWeightsPrevPrev);
    const double normGlobal = norm(deltaGlobal);
    if (normGlobal < 1e-12)
    {
        Logger::log(LogLevel::WARNING,
            "[BackdoorDefense] Global model delta is near-zero (model may have converged). "
            "Skipping filter.");
        result.acceptedIDs = clientIDs;
        return result;
    }
    const size_t numClients = weightsList.size();
    std::vector<std::vector<float>> deltas(numClients);
    std::vector<double> deltaNorms(numClients);
    std::vector<float>  cosineScores(numClients);
    for (size_t c = 0; c < numClients; ++c)
    {
        deltas[c]      = subtract(weightsList[c], globalWeightsPrev);
        deltaNorms[c]  = norm(deltas[c]);
        const double denom = deltaNorms[c] * normGlobal;
        if (denom < 1e-12)
            cosineScores[c] = 0.0f;
        else
            cosineScores[c] = static_cast<float>(dot(deltas[c], deltaGlobal) / denom);
        Logger::log(LogLevel::INFO,
            "[BackdoorDefense] Client " + std::to_string(clientIDs[c]) +
            " cosine score vs global history: " + std::to_string(cosineScores[c]));
    }
    std::vector<bool> keep(numClients, true);
    for (size_t c = 0; c < numClients; ++c)
    {
        if (cosineScores[c] < COSINE_THRESHOLD)
        {
            keep[c] = false;
            Logger::log(LogLevel::WARNING,
                "[BackdoorDefense] Client " + std::to_string(clientIDs[c]) +
                " REJECTED — cosine score " + std::to_string(cosineScores[c]) +
                " < threshold " + std::to_string(COSINE_THRESHOLD));
        }
    }
    if (std::none_of(keep.begin(), keep.end(), [](bool b){ return b; }))
    {
        size_t best = 0;
        for (size_t c = 1; c < numClients; ++c)
            if (cosineScores[c] > cosineScores[best]) best = c;
        keep[best] = true;
        Logger::log(LogLevel::WARNING,
            "[BackdoorDefense] All clients rejected — retaining best-scoring "
            "client " + std::to_string(clientIDs[best]) + " to avoid empty cohort.");
    }
    std::vector<float> survivorNorms;
    for (size_t c = 0; c < numClients; ++c)
        if (keep[c])
            survivorNorms.push_back(static_cast<float>(deltaNorms[c]));
    const float clipNorm = median(survivorNorms);
    if (clipNorm > 1e-12f)
    {
        for (size_t c = 0; c < numClients; ++c)
        {
            if (!keep[c]) continue;
            if (deltaNorms[c] > clipNorm)
            {
                const double scale = clipNorm / deltaNorms[c];
                auto &w = weightsList[c];
                for (size_t i = 0; i < w.size(); ++i)
                    w[i] = globalWeightsPrev[i] +
                           static_cast<float>(deltas[c][i] * scale);
            }
        }
    }
    std::vector<std::vector<float>> keptWeights;
    std::vector<uint32_t>           keptSamples;
    std::vector<uint32_t>           keptIDs;
    for (size_t c = 0; c < numClients; ++c)
    {
        if (keep[c])
        {
            keptWeights.push_back(std::move(weightsList[c]));
            keptSamples.push_back(sampleSizesList[c]);
            keptIDs.push_back(clientIDs[c]);
            result.acceptedIDs.push_back(clientIDs[c]);
        }
        else
        {
            result.rejectedIDs.push_back(clientIDs[c]);
        }
    }
    weightsList     = std::move(keptWeights);
    sampleSizesList = std::move(keptSamples);
    clientIDs       = std::move(keptIDs);
    result.clientsAfter = weightsList.size();
    Logger::log(LogLevel::INFO,
        "[BackdoorDefense] Round result: " +
        std::to_string(result.clientsBefore) + " received, " +
        std::to_string(result.clientsAfter)  + " accepted, " +
        std::to_string(result.rejectedIDs.size()) + " rejected. " +
        "clip_norm=" + std::to_string(clipNorm));
    return result;
}