#include "federated/watchdog.h"
#include "logger/logger.h"
#include "protocol/OP_CODES.h"
#include "protocol/protocol.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

Watchdog::Watchdog(SharedState& shared, RoundManager& roundManager)
    : shared(shared), roundManager(roundManager) {}

void Watchdog::start()
{
    if (running.exchange(true))
        return;

    watchdogThread = std::thread(&Watchdog::loop, this);
}

void Watchdog::stop()
{
    running = false;

    if (watchdogThread.joinable())
        watchdogThread.join();
}

bool Watchdog::pingClient(uint32_t id, int sock)
{
    std::lock_guard<std::mutex> ioLock(shared.socketIOMutex);

    timeval timeout{};
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    timeval noTimeout{};
    noTimeout.tv_sec = 0;
    noTimeout.tv_usec = 0;

    try
    {
        Protocol::sendMessage(sock, ServerOp::PING, "Ping");

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            return false;

        std::string state = Protocol::receiveState(sock);

        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &noTimeout, sizeof(noTimeout));

        Logger::log(
            LogLevel::DEBUG,
            "[Watchdog] Client " + std::to_string(id) +
            " sock " + std::to_string(sock) +
            " state: " + state
        );

        return true;
    }
    catch (...)
    {
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &noTimeout, sizeof(noTimeout));
        return false;
    }
}

void Watchdog::removeDeadClients(const std::vector<uint32_t>& deadClients, bool& shouldAggregate)
{
    shouldAggregate = false;

    std::scoped_lock lock(shared.statesMutex, shared.clientsMutex, shared.samplesMutex);

    for (uint32_t id : deadClients)
    {
        auto it = shared.clientMap.find(id);

        if (it != shared.clientMap.end())
        {
            close(it->second);
            shared.clientMap.erase(it);
        }

        shared.statesMap.erase(id);
        shared.samplesMap.erase(id);
    }

    if (!shared.trainingActive.load())
        return;

    if (shared.clientMap.empty())
    {
        Logger::log(
            LogLevel::ERROR,
            "[Watchdog] All clients were removed. Cannot aggregate."
        );

        shared.trainingActive = false;
        return;
    }

    bool allRemainingClientsFinished = std::all_of(
        shared.clientMap.begin(),
        shared.clientMap.end(),
        [this](const auto& pair)
        {
            const uint32_t id = pair.first;
            auto it = shared.statesMap.find(id);
            return it != shared.statesMap.end() && it->second == 0;
        }
    );

    shouldAggregate = allRemainingClientsFinished;
}

void Watchdog::loop()
{
    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        if (!running.load())
            break;

        if (!shared.trainingActive.load() || shared.aggregationStarted.load())
            continue;

        std::vector<std::pair<uint32_t, int>> trainingClients;

        {
            std::scoped_lock lock(shared.statesMutex, shared.clientsMutex);

            for (const auto& [id, sock] : shared.clientMap)
            {
                auto it = shared.statesMap.find(id);

                if (it != shared.statesMap.end() && it->second == 1)
                    trainingClients.push_back({id, sock});
            }
        }

        std::vector<uint32_t> deadClients;

        for (const auto& [id, sock] : trainingClients)
        {
            if (!running.load() || shared.aggregationStarted.load())
                break;

            bool stillTraining = false;
            {
                std::lock_guard<std::mutex> lock(shared.statesMutex);
                auto it = shared.statesMap.find(id);
                stillTraining = (it != shared.statesMap.end() && it->second == 1);
            }

            if (!stillTraining)
                continue;

            if (!pingClient(id, sock))
            {
                Logger::log(
                    LogLevel::WARNING,
                    "[Watchdog] Client " + std::to_string(id) +
                    " did not respond to PING. Removing."
                );

                deadClients.push_back(id);
            }
        }

        if (deadClients.empty())
            continue;

        bool shouldAggregate = false;
        removeDeadClients(deadClients, shouldAggregate);

        if (shouldAggregate)
            roundManager.tryAggregateAndContinue();
    }
}