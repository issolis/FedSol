#include "protocol/serverProtocol.h"
#include "datasetutils/datasetSplitter.h"
#include <thread>
#include <fstream>
#include "protocol/OP_CODES.h"

ServerProtocol::ServerProtocol(SharedState &shared) : shared(shared)
{
}

void ServerProtocol::sendStartTraining(int sock, const std::vector<float> &weights)
{
    Protocol::sendMessage(sock, ServerOp::START_TRAINING, "Start Training");
    Protocol::sendWeights(sock, weights);
}

void ServerProtocol::sendDataset(const std::string &datasetPath, SharedState &shared)
{
    size_t n = shared.clientCount();
    DatasetSplitter::split(datasetPath, n);
 
    std::vector<std::thread> threads;
 
    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        int i = 0;
        for (const auto &client : shared.clientMap)
        {
            int sockID = client.second;
            int clientIndex = i;
            threads.emplace_back([sockID, clientIndex, &datasetPath]()
                                 {
                std::string tarPath = "./input_server/dataset_train_"
                                      + std::to_string(clientIndex) + ".tar.gz";
                std::string npzPath = "./input_server/dataset_train_"
                                      + std::to_string(clientIndex) + ".npz";
 
                std::string filePath;
                std::ifstream tarCheck(tarPath);
                if (tarCheck.good())
                    filePath = tarPath;
                else
                    filePath = npzPath;
 
                Protocol::sendMessage(sockID, ServerOp::SEND_DATASET, "Sending dataset");
                Protocol::sendFile(sockID, filePath); });
            i++;
        }
    }
 
    for (auto &t : threads)
        t.join();
}

void ServerProtocol::HandShake(std::string &password, int clientSockID, const Architecture &global_arch)
{
    std::string response;

    if (!authManager.authenticate(password))
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(clientSockID) + "] Authentication failed");

        response = "AUTH_FAILED";
        Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, response);

        close(clientSockID);
        return;
    }

    response = "AUTH_SUCCESSFUL";
    Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, response);

    uint32_t id = Protocol::receiveID(clientSockID);
    Logger::log(LogLevel::INFO, "[Client " + std::to_string(id) + "] Authentication success");

    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        if (shared.clientMap.find(id) != shared.clientMap.end())
        {
            Logger::log(LogLevel::ERROR,
                        "[Client " + std::to_string(id) + "] Duplicate ID. Already connected.");

            Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, "ID_DUPLICATE");
            close(clientSockID);
            return;
        }
    }

    Architecture clientArch = Protocol::receiveArchitecture(clientSockID);

    if (!Architecture().verifyArchitecture(clientArch, global_arch))
    {
        Logger::log(LogLevel::ERROR,
                    "[Client " + std::to_string(id) + "] Architecture mismatch");

        std::string resp = "ARCH_INVALID";
        Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, resp);

        close(clientSockID);
        return;
    }

    {
        std::lock_guard<std::mutex> lockID(shared.clientsMutex);
        std::lock_guard<std::mutex> lockStates(shared.statesMutex);

        shared.statesMap[id] = 0;
        shared.clientMap[id] = clientSockID;
    }

    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(id) + "] Architecture verified");
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(id) + "] Registered");

    std::string resp = "ARCH_OK";
    Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, resp);

    if (shared.clientMap.size() == 1)
        Logger::log(LogLevel::INFO, "[MODE] READY");
}

void ServerProtocol::HandShakePT(std::string &password, int clientSockID, const std::string &globalHash)
{
    std::string response;

    if (!authManager.authenticate(password))
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(clientSockID) + "] Authentication failed");

        response = "AUTH_FAILED";
        Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, response);

        close(clientSockID);
        return;
    }

    response = "AUTH_SUCCESSFUL";
    Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, response);

    uint32_t id = Protocol::receiveID(clientSockID);
    Logger::log(LogLevel::INFO, "[Client " + std::to_string(id) + "] Authentication success");

    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        if (shared.clientMap.find(id) != shared.clientMap.end())
        {
            Logger::log(LogLevel::ERROR,
                        "[Client " + std::to_string(id) + "] Duplicate ID. Already connected.");

            Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, "ID_DUPLICATE");
            close(clientSockID);
            return;
        }
    }

    std::string clientHash = Protocol::receiveHash(clientSockID);

    if (clientHash != globalHash)
    {
        Logger::log(LogLevel::ERROR,
                    "[Client " + std::to_string(id) + "] PT hash mismatch — model rejected");

        Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, "HASH_INVALID");
        close(clientSockID);
        return;
    }

    {
        std::lock_guard<std::mutex> lockID(shared.clientsMutex);
        std::lock_guard<std::mutex> lockStates(shared.statesMutex);

        shared.statesMap[id] = 0;
        shared.clientMap[id] = clientSockID;
    }

    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(id) + "] PT hash verified");
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(id) + "] Registered");

    Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, "HASH_OK");

    if (shared.clientMap.size() == 1)
        Logger::log(LogLevel::INFO, "[MODE] READY");
}