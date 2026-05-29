#include "federated/aggregator.h"
#include <chrono>
#include "protocol/OP_CODES.h"

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

Aggregator::Aggregator(SharedState &shared, Model &globalModel)
    : shared(shared), globalModel(globalModel)
{
}

bool Aggregator::aggregate(const std::string &path)
{
    std::vector<std::pair<uint32_t, int>> clients;

    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        clients = {shared.clientMap.begin(), shared.clientMap.end()};
    }

    std::vector<std::vector<float>> weightsList;
    std::vector<uint32_t> sampleSizesList;
    std::vector<uint32_t> clientIDsList;

    std::mutex weightsMutex;
    std::vector<std::thread> threads;

    auto t_comm_start = Clock::now();

    for (const auto &[id, sock] : clients)
    {
        uint32_t clientID = id;
        int clientSock = sock;

        threads.emplace_back([&, clientID, clientSock]()
                             {
            try
            {
                std::vector<float> weights;
                {
                    std::lock_guard<std::mutex> ioLock(shared.socketIOMutex);
                    Protocol::sendMessage(clientSock, ServerOp::REQUEST_WEIGHTS, "Weights");
                    weights = Protocol::receiveWeights(clientSock);
                }
                size_t expectedSize = globalModel.getWeights().size();
                 if (weights.size() != expectedSize)
                {
                    Logger::log(LogLevel::ERROR,
                        "[Aggregator] Client " + std::to_string(clientID) +
                        " sent " + std::to_string(weights.size()) +
                        " weights, expected " + std::to_string(expectedSize) +
                        ". Discarding and removing client.");
                    
                    throw std::runtime_error("Invalid weight count from client " + std::to_string(clientID));
                }
                uint32_t sampleSize = 0;
                {
                    std::lock_guard<std::mutex> lock(shared.samplesMutex);
                    auto it = shared.samplesMap.find(clientID);
                    if (it == shared.samplesMap.end())
                    {
                        throw std::runtime_error("Sample size not found for client");
                    }
                    sampleSize = it->second;
                }

                {
                    std::lock_guard<std::mutex> lock(weightsMutex);
                    weightsList.push_back(weights);
                    sampleSizesList.push_back(sampleSize);
                    clientIDsList.push_back(clientID); 
                }
            }
            catch (...)
            {
                Logger::log(LogLevel::ERROR,
                    "[Aggregator] Client " + std::to_string(clientID) +
                    " failed during aggregation. Removing from state.");

                {
                    std::scoped_lock lock(shared.clientsMutex, shared.statesMutex, shared.samplesMutex);

                    auto it = shared.clientMap.find(clientID);
                    if (it != shared.clientMap.end())
                    {
                        close(it->second);
                        shared.clientMap.erase(it);
                    }

                    shared.statesMap.erase(clientID);
                    shared.samplesMap.erase(clientID);
                }
            } });
    }

    for (auto &t : threads)
        t.join();

    auto t_comm_end = Clock::now();

    {
        std::lock_guard<std::mutex> histLock(shared.globalHistoryMutex);
        BackdoorDefense::filter(
            shared.defenseEnabled.load(),
            shared.globalWeightsPrev,
            shared.globalWeightsPrevPrev,
            weightsList,
            sampleSizesList,
            clientIDsList);
    }

    if (weightsList.empty())
    {
        Logger::log(LogLevel::ERROR, "[Aggregator] No weights received. Aborting aggregation.");
        return false;
    }

    std::vector<float> weights = FedAvg::fedAvg(weightsList, sampleSizesList);

    auto t_agg_end = Clock::now();

    auto comm_ms = std::chrono::duration_cast<Ms>(t_comm_end - t_comm_start).count();
    auto agg_ms = std::chrono::duration_cast<Ms>(t_agg_end - t_comm_end).count();

    Logger::log(LogLevel::INFO,
                "[Aggregator] comm_time_ms: " + std::to_string(comm_ms));
    Logger::log(LogLevel::INFO,
                "[Aggregator] aggregation_time_ms: " + std::to_string(agg_ms));

    globalModel.setWeights(weights);

    {
        std::lock_guard<std::mutex> histLock(shared.globalHistoryMutex);
        shared.globalWeightsPrevPrev = shared.globalWeightsPrev;
        shared.globalWeightsPrev = weights;
    }
    
    JSONManager::updateWeightsInJSON(path, weights);
    ModelExporter::exportModel(path);

    return true;
}