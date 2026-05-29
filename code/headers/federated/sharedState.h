#ifndef SHAREDSTATE_H
#define SHAREDSTATE_H

#include <unordered_map>
#include <vector>
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
    std::vector<float> globalWeightsPrev;
    std::vector<float> globalWeightsPrevPrev;
    std::mutex globalHistoryMutex;

    size_t clientCount();
    std::string inferServerState();
    void printClientsStatus();
};

#endif