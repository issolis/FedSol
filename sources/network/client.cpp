#include "network/client.h"
#include <utility>
#include "network/connection.h"
#include "logger/logger.h"

Client::Client(unsigned short port, std::string serverIP, std::string password, Model &model, const std::string& path)
{
    this->port = port;
    this->serverIP = serverIP;
    this->password = password;
    this->model = &model;
    this->path = path; 
}

void Client::listener()
{
    int sockID = Connection::createConnection(serverIP, port);
    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Connected to server");
    if (!authenticate(sockID, password, model->getID()))
    {
        Logger::log(LogLevel::INFO,
                    "[Sock " + std::to_string(sockID) + "] Received weights. Starting training");
        close(sockID);
        return;
    }

    try
    {
        while (true)
        {
            Message msg = Protocol::receiveMessage(sockID);
            Logger::log(LogLevel::DEBUG,
                        "[Sock " + std::to_string(sockID) + "] Message content: " + msg.content);
            Logger::log(LogLevel::DEBUG,
                        "[Sock " + std::to_string(sockID) + "] Message code: " + std::to_string(msg.code));

            switch (msg.code)
            {
            case 1:
            {
                state = 1;

                std::vector<float> weights = Protocol::receiveWeights(sockID);

                {
                    std::lock_guard<std::mutex> lock(modelMutex);
                    model->setWeights(weights);
                    JSONManager::updateWeightsInJSON(path, weights);
                }
                Logger::log(LogLevel::INFO,
                            "[Sock " + std::to_string(sockID) + "] Received weights. Starting training");
                /// TRAINING

                reportTrainingEnded();
                break;
            }

            case 2:
            {
                std::vector<float> weights;
                {
                    std::lock_guard<std::mutex> lock(modelMutex);
                    weights = model->getWeights();
                }

                Protocol::sendWeights(sockID, weights);
                Logger::log(LogLevel::INFO,
                            "[Client " + std::to_string(model->getID()) + "] Sending weights to server");
                break;
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
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(sockID) + "] Disconnected from server: " + std::string(e.what()));

        close(sockID);
    }
}

void Client::setModel(Model &model)
{
    this->model = &model;
}

int Client::getState()
{
    return state;
}

void Client::reportTrainingEnded()
{
    state = 0;
    int sockID = Connection::createConnection(serverIP, port);
    Protocol::sendAuthMessage(sockID, 1, password, "Training Ended");
    Protocol::sendID(sockID, model->getID());
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(model->getID()) + "] Finished training");
    close(sockID);
}

void Client::run()
{
    Logger::init("logs/client_" + std::to_string(model->getID()) + ".log");
    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(model->getID()) + "] Finished training");
    std::thread(&Client::listener, this).detach();
    while (true)
        ;
}

bool Client::authenticate(int sockID, const std::string &password, uint32_t id)
{
    std::string content = "Auth";

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Sending authentication request");

    Protocol::sendAuthMessage(sockID, 0, password, content);

    Message msg = Protocol::receiveMessage(sockID);

    Logger::log(LogLevel::DEBUG,
                "[Sock " + std::to_string(sockID) + "] Server response: " + msg.content);

    if (msg.content == "AUTH_SUCCESSFUL")
    {
        Logger::log(LogLevel::INFO,
                    "[Sock " + std::to_string(sockID) + "] Authentication successful");

        Protocol::sendID(sockID, this->model->getID());

        Logger::log(LogLevel::DEBUG,
                    "[Client " + std::to_string(id) + "] Sent client ID");

        Protocol::sendArchitecture(sockID, this->model->getArchitecture());

        Logger::log(LogLevel::DEBUG,
                    "[Client " + std::to_string(id) + "] Sent architecture");

        Message msg = Protocol::receiveMessage(sockID);

        Logger::log(LogLevel::DEBUG,
                    "[Client " + std::to_string(id) + "] Architecture response: " + msg.content);

        if (msg.content == "ARCH_OK")
        {
            Logger::log(LogLevel::INFO,
                        "[Client " + std::to_string(id) + "] Architecture verified");

            return true;
        }
        else
        {
            Logger::log(LogLevel::ERROR,
                        "[Client " + std::to_string(id) + "] Architecture rejected by server");

            return false;
        }

        Logger::log(LogLevel::WARNING,
                    "[Client " + std::to_string(id) + "] Unknown architecture response");

        return false;
    }
    else
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(sockID) + "] Authentication rejected by server");

        return false;
    }

    Logger::log(LogLevel::WARNING,
                "[Sock " + std::to_string(sockID) + "] Unknown server response");

    return false;
}