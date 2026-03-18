#ifndef SHAREDSTATE_H
#define SHAREDSTATE_H

#include <unordered_map>
#include <atomic>
#include <unistd.h>
#include <mutex>

class SharedState {
public:
    SharedState(); 
    std::unordered_map<uint32_t, int> statesMap;
    std::unordered_map<uint32_t, int> clientMap;

    std::mutex statesMutex;
    std::mutex clientsMutex;

    std::atomic<bool> aggregationStarted{false};
};


#endif