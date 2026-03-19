#include "network/FLcoordinator.h"

FLCoordinator::FLCoordinator(SharedState &shared, Model &model)
    : shared(shared), globalModel(model)
{
}

void FLCoordinator::startTraining()
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

    for (const auto &[id, sock] : clients)
    {
        try
        {
            Protocol::sendMessage(sock, 1, "Start Training");

            std::vector<float> weights;
            {
                std::lock_guard<std::mutex> lock(modelMutex);
                weights = globalModel.getWeights();
            }

            Protocol::sendWeights(sock, weights);
        }
        catch (const std::exception &e)
        {
            std::cout << "[ERROR] Client " << id << " failed. Removing...\n";

            {
                std::lock_guard<std::mutex> lock1(shared.clientsMutex);
                std::lock_guard<std::mutex> lock2(shared.statesMutex);
                shared.clientMap.erase(id);
                shared.statesMap.erase(id);
            }
        }
    }
}

void FLCoordinator::aggregate(bool endTraining)
{
    std::vector<std::pair<uint32_t, int>> clients;

    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        clients = {shared.clientMap.begin(), shared.clientMap.end()};
    }

    std::vector<std::vector<float>> weightsList;
    std::vector<uint32_t> sizesList;

    std::mutex weightsMutex;
    std::vector<std::thread> threads;

    for (const auto &[id, sock] : clients)
    {
        uint32_t clientID = id;
        int clientSock = sock;

        threads.emplace_back([&, clientID, clientSock]()
                             {
        try
        {
            Protocol::sendMessage(clientSock, 2, "Weights");

            std::vector<float> weights = Protocol::receiveWeights(clientSock);

            {
                std::lock_guard<std::mutex> lock(weightsMutex);
                weightsList.push_back(weights);
                sizesList.push_back(weights.size());
            }
        }
        catch (...)
        {
            std::cout << "[ERROR] Client " << clientID << " failed during aggregation\n";

            {
                std::lock_guard<std::mutex> lock1(shared.clientsMutex);
                std::lock_guard<std::mutex> lock2(shared.statesMutex);
                shared.clientMap.erase(clientID);
                shared.statesMap.erase(clientID);
            }
        } });
    }

    for (auto &t : threads)
        t.join();

    if (weightsList.empty())
    {
        std::cout << "[ERROR] No weights received\n";
        return;
    }

    std::vector<float> weights = FedAvg::fedAvg(weightsList, sizesList);

    {
        std::lock_guard<std::mutex> lock(modelMutex);
        globalModel.setWeights(weights);
    }

    {
        std::lock_guard<std::mutex> lock(shared.statesMutex);
        for (auto &[id, state] : shared.statesMap)
            state = 1;
    }

    shared.aggregationStarted = false;
    if (!endTraining)
        startTraining();
}