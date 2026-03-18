#include "network/server.h"

Server::Server(unsigned short port, uint32_t backlog, Model &globalModel) : globalModel(globalModel), coordinator(shared, globalModel)
{
    Logger::init("logs/server.log");
    server_fd = Connection::createServerConnection(backlog, port);
    Logger::log(LogLevel::INFO, "Client starting...");
    serializer = Serializer();
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
            coordinator.aggregate(epochs == 0);
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
        std::cout << "\n===== SERVER MENU =====\n";
        std::cout << "1. Start Training\n";
        std::cout << "2. Clients listed\n";
        std::cout << "3. Exit\n";
        std::cout << "Option: ";

        std::cin >> option;

        switch (option)
        {
        case 1:
            std::cout << "Number of epochs: ";
            std::cin >> epochs;
            coordinator.startTraining();
            break;
        default:
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
