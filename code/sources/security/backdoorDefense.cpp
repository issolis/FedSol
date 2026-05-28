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

double BackdoorDefense::norm(const std::vector<float> &a)
{
    return std::sqrt(dot(a, a));
}

float BackdoorDefense::median(std::vector<float> values)
{
    if (values.empty())
        return 0.0f;
    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if (values.size() % 2 == 0)
        return 0.5f * (values[mid - 1] + values[mid]);
    return values[mid];
}

BackdoorDefense::Result BackdoorDefense::filter(
    bool enabled,
    const std::vector<float> &previousGlobalWeights,
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
            "[BackdoorDefense] Disabled by config. Passing all "
            + std::to_string(weightsList.size()) + " client updates through.");
        result.acceptedIDs = clientIDs;
        return result;
    }

    result.applied = true;

    const size_t numClients = weightsList.size();

    if (numClients < 3)
    {
        Logger::log(LogLevel::WARNING,
            "[BackdoorDefense] Only " + std::to_string(numClients) +
            " client(s) present; need >= 3 for outlier detection. Skipping filter.");
        result.acceptedIDs = clientIDs;
        return result;
    }

    const bool haveReference =
        !previousGlobalWeights.empty() &&
        previousGlobalWeights.size() == weightsList[0].size();

    std::vector<std::vector<float>> updates(numClients);
    for (size_t c = 0; c < numClients; ++c)
    {
        const auto &w = weightsList[c];
        updates[c].resize(w.size());
        if (haveReference)
            for (size_t i = 0; i < w.size(); ++i)
                updates[c][i] = w[i] - previousGlobalWeights[i];
        else
            updates[c] = w;
    }

    std::vector<double> updateNorms(numClients);
    for (size_t c = 0; c < numClients; ++c)
        updateNorms[c] = norm(updates[c]);

    std::vector<float> avgCosine(numClients, 0.0f);
    for (size_t a = 0; a < numClients; ++a)
    {
        double accum = 0.0;
        int counted = 0;
        for (size_t b = 0; b < numClients; ++b)
        {
            if (a == b) continue;
            const double denom = updateNorms[a] * updateNorms[b];
            double cos = 0.0;
            if (denom > 1e-12)
                cos = dot(updates[a], updates[b]) / denom;
            accum += cos;
            ++counted;
        }
        avgCosine[a] = counted > 0 ? static_cast<float>(accum / counted) : 1.0f;
    }


    const float medCosine = median(avgCosine);
    const float relativeFloor = medCosine - 0.5f * (medCosine - COSINE_OUTLIER_THRESHOLD);

    std::vector<bool> keep(numClients, true);
    for (size_t c = 0; c < numClients; ++c)
    {
        if (avgCosine[c] < COSINE_OUTLIER_THRESHOLD || avgCosine[c] < relativeFloor)
        {
            keep[c] = false;
            Logger::log(LogLevel::WARNING,
                "[BackdoorDefense] Client " + std::to_string(clientIDs[c]) +
                " flagged as backdoor suspect (avg cosine = " +
                std::to_string(avgCosine[c]) + ", cohort median = " +
                std::to_string(medCosine) + ").");
        }
    }


    if (std::none_of(keep.begin(), keep.end(), [](bool b){ return b; }))
    {
        size_t best = 0;
        for (size_t c = 1; c < numClients; ++c)
            if (avgCosine[c] > avgCosine[best]) best = c;
        keep[best] = true;
        Logger::log(LogLevel::WARNING,
            "[BackdoorDefense] All clients flagged; retaining most-aligned "
            "client " + std::to_string(clientIDs[best]) + " to avoid empty cohort.");
    }

    std::vector<float> survivorNorms;
    survivorNorms.reserve(numClients);
    for (size_t c = 0; c < numClients; ++c)
        if (keep[c])
            survivorNorms.push_back(static_cast<float>(updateNorms[c]));

    const float clipNorm = median(survivorNorms);

    if (haveReference && clipNorm > 1e-12f)
    {
        for (size_t c = 0; c < numClients; ++c)
        {
            if (!keep[c]) continue;
            const double n = updateNorms[c];
            if (n > clipNorm)
            {
                const double scale = clipNorm / n;
                auto &w = weightsList[c];
                for (size_t i = 0; i < w.size(); ++i)
                    w[i] = previousGlobalWeights[i] +
                           static_cast<float>(updates[c][i] * scale);
            }
        }
    }

    std::vector<std::vector<float>> keptWeights;
    std::vector<uint32_t> keptSamples;
    std::vector<uint32_t> keptIDs;
    keptWeights.reserve(numClients);
    keptSamples.reserve(numClients);
    keptIDs.reserve(numClients);

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
        "[BackdoorDefense] Aggregation cohort: " +
        std::to_string(result.clientsBefore) + " received, " +
        std::to_string(result.clientsAfter) + " accepted, " +
        std::to_string(result.rejectedIDs.size()) + " rejected. clip_norm = " +
        std::to_string(clipNorm) + ".");

    return result;
}