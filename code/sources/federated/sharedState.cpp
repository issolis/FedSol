#include "federated/sharedState.h"
#include <iostream>

SharedState::SharedState() {}

std::string SharedState::inferServerState()
{
    std::lock(statesMutex, clientsMutex);
    std::lock_guard<std::mutex> lockStates(statesMutex, std::adopt_lock);
    std::lock_guard<std::mutex> lockClients(clientsMutex, std::adopt_lock);

    // Aggregation has the highest priority as a global server state
    if (aggregationStarted.load())
    {
        return "AGGREGATING";
    }

    // If there are no connected clients, the server is idle
    if (clientMap.empty())
    {
        return "IDLE";
    }

    bool allFinished = true;
    bool allTraining = true;
    bool hasFinished = false;
    bool hasTraining = false;

    for (const auto& [id, sockID] : clientMap)
    {
        auto it = statesMap.find(id);

        // A connected client without a known state is inconsistent
        if (it == statesMap.end())
        {
            return "INCONSISTENT";
        }

        int state = it->second;

        if (state == 0)
        {
            hasFinished = true;
            allTraining = false;
        }
        else if (state == 1)
        {
            hasTraining = true;
            allFinished = false;
        }
        else
        {
            return "INCONSISTENT";
        }
    }

    if (allFinished)
    {
        return "ALL_CLIENTS_FINISHED";
    }

    if (allTraining)
    {
        return "ALL_CLIENTS_TRAINING";
    }

    if (hasFinished && hasTraining)
    {
        return "MIXED";
    }

    return "INCONSISTENT";
}

void SharedState::printClientsStatus()
{
    std::lock(statesMutex, clientsMutex);
    std::lock_guard<std::mutex> lockStates(statesMutex, std::adopt_lock);
    std::lock_guard<std::mutex> lockClients(clientsMutex, std::adopt_lock);

    std::cout << "\n===== CLIENT STATUS =====\n";

    if (clientMap.empty())
    {
        std::cout << "No connected clients.\n";
        std::cout << "=========================\n";
        return;
    }

    for (const auto& [id, sockID] : clientMap)
    {
        std::string stateStr = "UNKNOWN";

        auto it = statesMap.find(id);
        if (it != statesMap.end())
        {
            if (it->second == 0)
            {
                stateStr = "FINISHED";
            }
            else if (it->second == 1)
            {
                stateStr = "TRAINING";
            }
        }

        std::cout << "Client ID: " << id
                  << " | Socket ID: " << sockID
                  << " | State: " << stateStr
                  << "\n";
    }

    std::cout << "=========================\n";
}

size_t SharedState::clientCount() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    return clientMap.size();
}