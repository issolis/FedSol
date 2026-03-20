#include "network/server.h"
#include "weightsUtils/weightsUtils.h"
#include <thread>
#include <chrono>


Server::Server(unsigned short port, uint32_t backlog, Model &globalModel, const std::string &path) : globalModel(globalModel), coordinator(shared, globalModel)
{
    Logger::init("logs/server.log");
    server_fd = Connection::createServerConnection(backlog, port);
    Logger::log(LogLevel::INFO, "Client starting...");
    serializer = Serializer();
    this->path = path; 
}

void Server::run()
{

    std::thread(&Server::consoleLoop, this).detach();
    while (true)
    {
        int clientSock = accept(server_fd, nullptr, nullptr);
        if (clientSock < 0)
            continue;

        std::thread(&Server::handleClient, this, clientSock).detach();
    }
}

void Server::handleClient(int clientSockID)
{

    try
    {
        Logger::log(LogLevel::INFO, "Client connected with socket id: " + std::to_string(clientSockID));
        AuthMessage message = Protocol::receiveAuthMessage(clientSockID);

        Logger::log(LogLevel::DEBUG, "Code: " + std::to_string(message.code));
        Logger::log(LogLevel::DEBUG, "Content: " + message.content);

        handleMessage(message, clientSockID);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

void Server::handleMessage(AuthMessage &message, int clientSockID)
{
    switch (message.code)
    {
    case 0:
    {
        authenticate(message.password, clientSockID);
        break;
    }

    case 1:
    {
        uint32_t id = Protocol::receiveID(clientSockID);
        Logger::log(LogLevel::INFO,
                    "[Client " + std::to_string(id) + "] Finished training for epoch");

        bool ready = false;

        {
            std::lock_guard<std::mutex> lock(shared.statesMutex);

            if (shared.statesMap.find(id) != shared.statesMap.end())
            {
                shared.statesMap[id] = 0;
            }
            else
            {
                Logger::log(LogLevel::WARNING, "Unknown client ID: " + std::to_string(id));
            }

            ready = std::all_of(
                shared.statesMap.begin(),
                shared.statesMap.end(),
                [](const auto &pair)
                {
                    return pair.second == 0;
                });
        }

        close(clientSockID);

        if (ready && !shared.aggregationStarted.exchange(true))
        {
            Logger::log(LogLevel::INFO, "All clients finished. Aggregating...");
            epochs -= 1;
            Logger::log(LogLevel::INFO,
                        "[SERVER] Completed one epoch. Remaining: " + std::to_string(epochs));
            coordinator.aggregate(epochs == 0, path);
        }

        break;
    }

    default:
    {
        Logger::log(LogLevel::WARNING, "Unknown message code");
        close(clientSockID);
        break;
    }
    }
}

void Server::authenticate(std::string &password, int clientSockID)
{
    std::string response;

    if (!authManager.authenticate(password))
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(clientSockID) + "] Authentication failed");

        response = "AUTH_FAILED";
        Protocol::sendMessage(clientSockID, 0, response);

        close(clientSockID);
        return;
    }

    response = "AUTH_SUCCESSFUL";
    Protocol::sendMessage(clientSockID, 0, response);

    uint32_t id = Protocol::receiveID(clientSockID);
    Logger::log(LogLevel::INFO, "[Client " + std::to_string(id) + "] Authentication success");
    Architecture clientArch = Protocol::receiveArchitecture(clientSockID);

    if (!verifyArchitecture(clientArch))
    {
        Logger::log(LogLevel::ERROR,
                    "[Client " + std::to_string(id) + "] Architecture mismatch");

        std::string resp = "ARCH_INVALID";
        Protocol::sendMessage(clientSockID, 0, resp);

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
    Protocol::sendMessage(clientSockID, 0, resp);
}

void Server::consoleLoop()
{
    while (true)
    {
        int option;

        std::cout << "\n===== SERVER MENU =====" << std::endl;
        std::cout << "1. Start Training" << std::endl;
        std::cout << "2. List Clients" << std::endl;
        std::cout << "3. Populate Random Weights" << std::endl;
        std::cout << "4. Exit" << std::endl;
        std::cout << "Option: ";

        if (!(std::cin >> option))
        {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "[ERROR] Invalid input." << std::endl;
            continue;
        }

        switch (option)
        {
        case 1:
        {
            bool validWeights = WeightUtils::validateWeights(
                this->globalModel.getArchitecture(),
                this->globalModel.getWeights());

            if (!validWeights)
            {
                size_t expected = WeightUtils::computeExpectedWeights(
                    this->globalModel.getArchitecture());

                std::cout << "[WARNING] Cannot start training." << std::endl;
                std::cout << "[WARNING] Weights do not match architecture." << std::endl;
                std::cout << "[WARNING] Expected: " << expected << std::endl;
                std::cout << "[WARNING] Current: " << this->globalModel.getWeights().size() << std::endl;
                std::cout << "[WARNING] Use option 3 to populate random weights first." << std::endl;

                break;
            }

            std::cout<<("Number of epochs: ");

            if (!(std::cin >> epochs))
            {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                Logger::log(LogLevel::ERROR, "Invalid epoch value.");
                break;
            }

            char confirm;
            std::cout << "Start training with " << epochs << " epochs? (y/n): ";
            std::cin >> confirm;

            if (confirm == 'y' || confirm == 'Y')
            {
                Logger::log(LogLevel::INFO, "Starting training...");
                std::cout << "[INFO] Starting training..." << std::endl;
                coordinator.startTraining();
            }
            else
            {
                std::cout << "Training cancelled." << std::endl;
                Logger::log(LogLevel::INFO, "Training cancelled.");
            }

            break;
        }

        case 2:
        {
            // coordinator.listClients(); // o el método que tengas

            std::this_thread::sleep_for(std::chrono::seconds(2));
            break;
        }

        case 3:
        {
            size_t expected = WeightUtils::computeExpectedWeights(
                this->globalModel.getArchitecture());

            char confirm;
            Logger::log(LogLevel::WARNING,
                        "overwrite current model weights.");
            std::cout << "[WARNING] This will overwrite current model weights." << std::endl;
            std::cout << "[INFO] Expected weights count: " << expected << std::endl;
            std::cout << "[INFO] Continue? (y/n): ";

            std::cin >> confirm;

            if (confirm == 'y' || confirm == 'Y')
            {
                std::vector<float> randomWeights =
                    WeightUtils::generateRandomWeights(expected);
                this->globalModel.setWeights(randomWeights);

                Logger::log(LogLevel::INFO,
                            "Random weights generated and loaded into model.");
                std::cout << "[INFO] Random weights generated and loaded into model." << std::endl;
            }
            else
            {
                Logger::log(LogLevel::INFO, "Random population cancelled.");
                std::cout << "[INFO] Random population cancelled." << std::endl;
            }

            break;
        }

        case 4:
        {
            char confirm;
            std::cout << "Are you sure you want to exit? (y/n): ";
            std::cin >> confirm;

            if (confirm == 'y' || confirm == 'Y')
            {
                Logger::log(LogLevel::INFO, "Server shutting down...");
                std::cout << "Server shutting down..." << std::endl;
                return;
            }
            else
            {
                std::cout << "Exit cancelled." << std::endl;
            }

            break;
        }

        default:
            std::cout << "[ERROR] Invalid option." << std::endl;
            break;
        }
    }
}

void Server::removeClient(uint32_t id)
{
    {
        std::lock_guard<std::mutex> lock(shared.clientsMutex);
        shared.clientMap.erase(id);
    }

    {
        std::lock_guard<std::mutex> lock(shared.statesMutex);
        shared.statesMap.erase(id);
    }
}

bool Server::verifyArchitecture(const Architecture &clientArch)
{
    Architecture arch = globalModel.getArchitecture();
    return clientArch.equals(arch);
}
