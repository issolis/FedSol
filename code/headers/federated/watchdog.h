#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "federated/sharedState.h"
#include "federated/roundManager.h"

class Watchdog
{
private:
    SharedState& shared;
    RoundManager& roundManager;
    std::thread watchdogThread;
    std::atomic<bool> running{false};

    void loop();
    bool pingClient(uint32_t id, int sock);
    void removeDeadClients(const std::vector<uint32_t>& deadClients, bool& shouldAggregate);

public:
    Watchdog(SharedState& shared, RoundManager& roundManager);
    void start();
    void stop();
};

#endif