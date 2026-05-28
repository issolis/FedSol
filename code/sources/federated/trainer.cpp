#include "federated/trainer.h"
#include "protocol/serverProtocol.h"

#include <vector>
#include <utility>
#include <iostream>
#include <exception>
#include <mutex>

Trainer::Trainer(SharedState &shared, Model &globalModel)
    : shared(shared), globalModel(globalModel)
{
}

void Trainer::startTraining()
{
    std::vector<std::pair<uint32_t, int>> clients;


    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        clients = {shared.clientMap.begin(), shared.clientMap.end()};
    }

    {
        std::lock_guard<std::mutex> lock(shared.statesMutex);
        for (const auto &[id, _] : clients)
        {
            shared.statesMap[id] = 1;
        }
    }

    if (clients.empty())
    {
        shared.trainingActive = false;
        Logger::log(LogLevel::WARNING, "[Trainer] No clients available to start training.");
        return;
    }

    shared.trainingActive = true;
    size_t startedClients = 0;
    std::vector<float> weights = globalModel.getWeights();

    for (const auto &[id, sock] : clients)
    {
        try
        {
            {
                std::lock_guard<std::mutex> ioLock(shared.socketIOMutex);
                ServerProtocol::sendStartTraining(sock, weights);
            }

            {
                std::lock_guard<std::mutex> lock(shared.statesMutex);
                shared.statesMap[id] = 1;
            }

            ++startedClients;
        }
        catch (const std::exception &e)
        {
            Logger::log(LogLevel::ERROR, "Client " + std::to_string(id) + " failed. Removing..");
            std::cout << "[ERROR] Client " << id
                      << " failed. Removing...\n";

            {
                std::scoped_lock lock(shared.clientsMutex, shared.statesMutex, shared.samplesMutex);

                auto it = shared.clientMap.find(id);
                if (it != shared.clientMap.end())
                {
                    close(it->second);
                    shared.clientMap.erase(it);
                }

                shared.statesMap.erase(id);
                shared.samplesMap.erase(id);
            }
        }
    }
    if (startedClients == 0)
    {
        shared.trainingActive = false;
        Logger::log(LogLevel::ERROR, "[Trainer] Training could not start for any client.");
    }
}