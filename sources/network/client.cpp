#include "network/client.h"
#include <utility>
#include "network/connection.h"

Client::Client(unsigned short port, std::string serverIP, std::string password, Model &model)
{
    this->port = port;
    this->serverIP = serverIP;
    this->password = password;
    this->model = model;
}

void Client::listener()
{
    int sockID = Connection::createConnection(serverIP, port);

    if (!authenticate(sockID, password, model.getID()))
    {
        std::cout << "Login failed\n";
        close(sockID);
        return;
    }

    try
    {
        while (true)
        {
            Message msg = Protocol::receiveMessage(sockID);
            std::cout << msg.content << std::endl;

            switch (msg.code)
            {
            case 1:
            {
                state = 1;

                std::vector<float> weights = Protocol::receiveWeights(sockID);
                std::cout<< weights[0] << std::endl; 

                {
                    std::lock_guard<std::mutex> lock(modelMutex);
                    model.setWeights(weights);
                }
                ///TRAINING

                reportTrainingEnded();
                break;
            }

            case 2:
            {
                std::vector<float> weights;
                {
                    std::lock_guard<std::mutex> lock(modelMutex);
                    weights = model.getWeights();
                }

                Protocol::sendWeights(sockID, weights);
                break;
            }

            default:
                std::cout << "Unknown message code\n";
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "Disconnected from server: " << e.what() << std::endl;
        close(sockID);
    }
}

void Client::setModel(Model &model)
{
    this->model = model;
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
    Protocol::sendID(sockID, model.getID());
    close(sockID);
}

void Client::run(){
    std::thread(&Client::listener, this).detach();
    while(true); 
}

bool Client::authenticate(int sockID, const std::string &password, uint32_t id)
{
    std::string content = "Auth";
    Protocol::sendAuthMessage(sockID, 0, password, content);
    Message msg = Protocol::receiveMessage(sockID);

    std::cout << "Server response: " << msg.content << std::endl;

    if (msg.content == "AUTH_SUCCESSFUL")
    {
        Protocol::sendID(sockID, this->model.getID());
        Protocol::sendArchitecture(sockID, this->model.getArchitecture());
        Message msg = Protocol::receiveMessage(sockID);

        if (msg.content == "ARCH_OK")
        {
            std::cout << "Architecture accepted\n";
            return true;
        }

        else
        {
            std::cout << "Architecture rejected by server\n";
            return false;
        }

        std::cout << "Unknown architecture response\n";
        return false;
    }
    else
    {
        std::cout << "Authentication rejected by server\n";
        return false;
    }

    std::cout << "Unknown server response\n";
    return false;
}