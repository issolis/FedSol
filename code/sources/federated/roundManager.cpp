#include "federated/roundManager.h"
#include "protocol/protocol.h"
#include "logger/logger.h"

#include <algorithm>
#include <unistd.h>
#include <mutex>
#include <chrono>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

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

    auto t_training_end = std::chrono::steady_clock::now();
    long long training_ms = 0;
    {
        std::lock_guard<std::mutex> lock(shared.roundTimeMutex);
        training_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            t_training_end - shared.roundStartTime).count();
    }
    Logger::log(LogLevel::INFO,
        "[RoundTimer] training_time_ms: " + std::to_string(training_ms));
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

        auto t_round_end = std::chrono::steady_clock::now();
        long long round_ms = 0;
        {
            std::lock_guard<std::mutex> lock(shared.roundTimeMutex);
            round_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                t_round_end - shared.roundStartTime).count();
        }
        Logger::log(LogLevel::INFO,
            "[RoundTimer] round_total_ms: " + std::to_string(round_ms));

        {
            std::time_t now_t = std::time(nullptr);
            std::tm tm_buf;
            localtime_r(&now_t, &tm_buf);
            std::ostringstream ts_ss;
            ts_ss << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S");
            std::string ts_str = ts_ss.str();

            mkdir("output_server", 0755);
            mkdir("output_server/globalResults", 0755);
            mkdir("output_server/globalResults/stats", 0755);

            std::string fpath = "output_server/globalResults/stats/timing_" + ts_str + ".json";
            std::ofstream tf(fpath);
            tf << "{\n";
            tf << "  \"timestamp\": \"" << ts_str << "\",\n";
            tf << "  \"training_time_ms\": " << training_ms << ",\n";
            tf << "  \"comm_time_ms\": "     << shared.lastCommMs.load() << ",\n";
            tf << "  \"agg_time_ms\": "      << shared.lastAggMs.load()  << ",\n";
            tf << "  \"round_total_ms\": "   << round_ms << ",\n";
            tf << "  \"clients\": {\n";
            std::lock_guard<std::mutex> sLock(shared.samplesMutex);
            bool first = true;
            for (const auto &[cid, nsamp] : shared.samplesMap) {
                if (!first) tf << ",\n";
                tf << "    \"" << cid << "\": " << nsamp;
                first = false;
            }
            tf << "\n  }\n}\n";
            Logger::log(LogLevel::INFO, "[RoundTimer] Timing saved -> " + fpath);
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