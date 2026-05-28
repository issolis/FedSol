#include "federated/roundManager.h"
#include "protocol/protocol.h"
#include "logger/logger.h"

#include <algorithm>
#include <unistd.h>
#include <mutex>

RoundManager::RoundManager(
    SharedState &shared,
    Trainer &trainer,
    Aggregator &aggregator)
    : shared(shared),
      trainer(trainer),
      aggregator(aggregator)
{
}

void RoundManager::handleTrainingFinished(int clientSockID, const std::string &path)
{
    uint32_t id = Protocol::receiveID(clientSockID);
    uint32_t samplesSize = Protocol::receiveSamplesSize(clientSockID);

    this->path = path;

    bool ready = false;

    {
        std::scoped_lock lock(shared.statesMutex, shared.samplesMutex);

        auto it = shared.statesMap.find(id);

        if (it == shared.statesMap.end())
        {
            Logger::log(
                LogLevel::WARNING,
                "Unknown client ID: " + std::to_string(id));

            close(clientSockID);
            return;
        }

        shared.samplesMap[id] = samplesSize;
        it->second = 0;

        Logger::log(
            LogLevel::INFO,
            "[Client " + std::to_string(id) +
                "] Finished training. Sample size: " + std::to_string(samplesSize));

        ready = std::all_of(
            shared.statesMap.begin(),
            shared.statesMap.end(),
            [](const auto &pair)
            {
                return pair.second == 0;
            });
    }

    close(clientSockID);

    if (ready)
    {
        tryAggregateAndContinue();
    }
}

bool RoundManager::markClientAsFinished(uint32_t id)
{
    std::lock_guard<std::mutex> lock(shared.statesMutex);

    auto it = shared.statesMap.find(id);

    if (it != shared.statesMap.end())
    {
        it->second = 0;
    }
    else
    {
        Logger::log(
            LogLevel::WARNING,
            "Unknown client ID: " + std::to_string(id));
        return false;
    }

    return std::all_of(
        shared.statesMap.begin(),
        shared.statesMap.end(),
        [](const auto &pair)
        {
            return pair.second == 0;
        });
}

void RoundManager::registerClientSampleSize(uint32_t id, uint32_t sampleSize)
{
    std::lock_guard<std::mutex> lock(shared.samplesMutex);

    shared.samplesMap[id] = sampleSize;

    Logger::log(
        LogLevel::INFO,
        "[Client " + std::to_string(id) +
            "] Registered sample size: " + std::to_string(sampleSize));
}

void RoundManager::tryAggregateAndContinue()
{
    if (shared.aggregationStarted.exchange(true))
    {
        return;
    }

    Logger::log(LogLevel::INFO, "All clients finished. Aggregating...");

    bool ok = aggregator.aggregate(path);

    if (ok)
    {
        uint32_t remainingEpochs = 0;

        uint32_t previousEpochs = shared.epochs.load();
        if (previousEpochs > 0)
        {
            remainingEpochs = shared.epochs.fetch_sub(1) - 1;
        }

        Logger::log(
            LogLevel::INFO,
            "[SERVER] Completed one epoch. Remaining: " + std::to_string(remainingEpochs));

        if (remainingEpochs > 0)
        {
            trainer.startTraining();
        }
        else
        {
            shared.trainingActive = false;
            Logger::log(LogLevel::INFO, "[SERVER] Training finished.");
        }
    }
    else
    {
        shared.trainingActive = false;
        Logger::log(LogLevel::ERROR, "[SERVER] Aggregation failed.");
    }

    shared.aggregationStarted = false;
}