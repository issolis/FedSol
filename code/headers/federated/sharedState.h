#ifndef SHAREDSTATE_H
#define SHAREDSTATE_H

#include <unordered_map>
#include <vector>
#include <chrono>
#include <atomic>
#include <unistd.h>
#include <mutex>
#include <string>
#include <cstdint>

class SharedState
{
public:
    SharedState();

    std::unordered_map<uint32_t, int> statesMap;
    std::unordered_map<uint32_t, int> clientMap;
    std::unordered_map<uint32_t, uint32_t> samplesMap;

    std::mutex samplesMutex;
    std::mutex statesMutex;
    std::mutex clientsMutex;

    std::atomic<uint32_t> epochs{0};
    std::atomic<bool> aggregationStarted{false};
    std::atomic<bool> trainingActive{false};

    std::mutex socketIOMutex;

    std::atomic<bool> defenseEnabled{false};
    std::atomic<long long> lastCommMs{0};
    std::atomic<long long> lastAggMs{0};
    std::chrono::steady_clock::time_point roundStartTime;
    std::mutex roundTimeMutex;
    std::vector<float> globalWeightsPrev;
    std::vector<float> globalWeightsPrevPrev;
    std::mutex globalHistoryMutex;

    size_t clientCount();
    std::string inferServerState();
    void printClientsStatus();
};

#endif