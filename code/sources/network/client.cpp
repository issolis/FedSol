#include "network/client.h"
#include <utility>
#include "network/connection.h"
#include "logger/logger.h"
#include "protocol/clientProtocol.h"
#include "localTraining/localTraining.h"
#include "protocol/OP_CODES.h"

Client::Client(unsigned short port, std::string serverIP, std::string password, Model &model, const std::string &path, const std::string &datasetPath, const std::string &ptHash) : model(model)
{
    this->port = port;
    this->serverIP = serverIP;
    this->password = password;
    this->path = path;
    this->datasetPath = datasetPath;
    this->ptHash = ptHash;
}

void Client::listener()
{
    int sockID;
    try
    {
        // ── Choose handshake based on mode ────────────────────────────────────
        std::cout<<ptHash<<std::endl; 
        if (!this->ptHash.empty())
        {
            // pt_path mode: validate by .pt hash instead of architecture
            std::cout<<ptHash<<std::endl; 
            sockID = ClientProtocol::connectAndAuthenticatePT(
                this->password, this->model, this->serverIP, this->port, this->ptHash);
        }
        else
        {
            // Normal mode: validate by architecture
            sockID = ClientProtocol::connectAndAuthenticate(
                this->password, this->model, this->serverIP, this->port);
        }

        while (true)
        {
            Message msg = Protocol::receiveMessage(sockID);
            Logger::log(LogLevel::DEBUG,
                        "[Sock " + std::to_string(sockID) + "] Message content: " + msg.content);
            Logger::log(LogLevel::DEBUG,
                        "[Sock " + std::to_string(sockID) + "] Message code: " + std::to_string(msg.code));

            switch (msg.code)
            {
            case ServerOp::START_TRAINING:
            {
                startLocalTraining(sockID);
                break;
            }

            case ServerOp::REQUEST_WEIGHTS:
            {
                sendWeights(sockID);
                break;
            }
            case ServerOp::SEND_DATASET:
            {
                receiveDataset(sockID);
                break;
            }
            case ServerOp::PING:
            {
                sendState(sockID);
                break;
            }
            case ServerOp::SHUTDOWN:
            {
                Logger::log(LogLevel::INFO,
                            "[Client " + std::to_string(model.getID()) + "] Shutdown received. Exiting.");
                close(sockID);
                exit(0);
            }

            default:
                Logger::log(LogLevel::WARNING,
                            "[Sock " + std::to_string(sockID) + "] Unknown message code");
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        int errorSockID = Connection::createConnection(this->serverIP, this->port);
        Logger::log(LogLevel::ERROR,
                    "[Client " + std::to_string(model.getID()) + "] Disconnected from server: " + std::string(e.what()));
        Protocol::sendError(errorSockID, "Client::listener", e.what(), this->password);
        close(sockID);
        close(errorSockID);
    }
}

void Client::sendState(int sockID)
{
    std::string stateStr = (state == ClientState::TRAINING) ? "TRAINING" : "IDLE";
    Protocol::sendState(sockID, stateStr);
    Logger::log(LogLevel::DEBUG,
                "[Client " + std::to_string(model.getID()) + "] PONG: " + stateStr);
}

void Client::startLocalTraining(int sockID)
{
    std::vector<float> weights = Protocol::receiveWeights(sockID);
    {
        std::lock_guard<std::mutex> lock(modelMutex);
        model.setWeights(weights);
        JSONManager::updateWeightsInJSON(path, weights);
    }

    state = ClientState::TRAINING;
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(model.getID()) + "] State: TRAINING");

    std::thread([this]()
                {
        LocalTraining::startLocalTraining(this->path, this->model);

        state = ClientState::IDLE;  
        Logger::log(LogLevel::INFO,
            "[Client " + std::to_string(model.getID()) + "] State: IDLE");

        LocalTraining::reportTrainingEnded(
            this->serverIP, this->password,
            this->path, this->port, this->model); })
        .detach();
}

void Client::sendWeights(int sockID)
{
    std::vector<float> weights;
    {
        std::lock_guard<std::mutex> lock(modelMutex);
        weights = model.getWeights();
    }

    Protocol::sendWeights(sockID, weights);
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(model.getID()) + "] Sending weights to server");
}

void Client::receiveDataset(int sockID)
{
    Logger::log(LogLevel::INFO, "[Client] Receiving dataset, saving to: " + datasetPath);
    Protocol::receiveFile(sockID, datasetPath);
    Logger::log(LogLevel::INFO, "[Client] Dataset received successfully");
}

void Client::run()
{
    Logger::init("output_client/logs/client_" + std::to_string(model.getID()) + ".log");
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(model.getID()) + "] Starting client");

    std::thread listenerThread(&Client::listener, this);
    listenerThread.join();
}