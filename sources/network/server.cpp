#include "network/server.h"


Server::Server(unsigned short port, uint32_t backlog, Model &globalModel)
{
    server_fd = Connection::createServerConnection(backlog, port); 
    std::cout << "Server listening on port " << port << std::endl;
    this->globalModel = globalModel;
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
        std::cout << "Client connected: " << clientSockID << std::endl;
        AuthMessage message = Protocol::receiveAuthMessage(clientSockID);

        std::cout << "[SERVER] Code: " << message.code << std::endl;
        std::cout << "[SERVER] Content: " << message.content << std::endl;

        handleMessage(message, clientSockID); 
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

void Server::handleMessage(AuthMessage& message, int clientSockID)
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
        std::cout << id << std::endl;

        bool ready = false;

        {
            std::lock_guard<std::mutex> lock(statesMutex);

            if (statesMap.find(id) != statesMap.end())
            {
                statesMap[id] = 0;
            }
            else
            {
                std::cout << "[SERVER] Unknown client ID: " << id << "\n";
            }

            ready = std::all_of(statesMap.begin(), statesMap.end(),
                                [](const auto &pair)
                                { return pair.second == 0; });
        }

        close(clientSockID);

        if (ready && !aggregationStarted.exchange(true))
        {
            std::cout << "[SERVER] All clients finished. Aggregating...\n";
            aggregate();
        }

        break;
    }

    default:
        std::cout << "[SERVER] Unknown message code" << std::endl;
        close(clientSockID);
        break;
    }
}

void Server::authenticate(std::string &password, int clientSockID)
{
    std::string response;

    if (!authManager.authenticate(password))
    {
        std::cout << "[SERVER] Authentication failed\n";

        response = "AUTH_FAILED";

        Protocol::sendMessage(clientSockID, 0, response);
        close(clientSockID);
        return;
    }

    std::cout << "[SERVER] Authentication success\n";
    response = "AUTH_SUCCESSFUL";
    Protocol::sendMessage(clientSockID, 0, response);
    uint32_t id = Protocol::receiveID(clientSockID);
    Architecture clientArch = Protocol::receiveArchitecture(clientSockID);

    if (!verifyArchitecture(clientArch))
    {
        std::cout << "[SERVER] Architecture mismatch\n";

        std::string resp = "ARCH_INVALID";
        Protocol::sendMessage(clientSockID, 0, resp);

        close(clientSockID);
        return;
    }
    {
        std::lock_guard<std::mutex> lockID(clientsMutex);
        std::lock_guard<std::mutex> lockStates(statesMutex);
        statesMap[id] = 0;
        clientMap[id] = clientSockID;
    }
    std::cout << "[SERVER] Client registered with id: " << id << std::endl;
    std::cout << "[SERVER] Architecture verified\n";

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
            startTraining();
            break;
        default:
            break;
        }
    }
}

void Server::startTraining()
{
    std::vector<std::pair<uint32_t, int>> clients;

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients = {clientMap.begin(), clientMap.end()};
    }

    {
        std::lock_guard<std::mutex> lock(statesMutex);
        for (const auto &[id, _] : clients)
        {
            statesMap[id] = 1;
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

            removeClient(id);
        }
    }
}

void Server::aggregate()
{
    std::vector<std::pair<uint32_t, int>> clients;

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients = {clientMap.begin(), clientMap.end()};
    }

    std::vector<std::vector<float>> weightsList;
    std::vector<uint32_t> sizesList;

    std::mutex weightsMutex;
    std::vector<std::thread> threads;

    for (const auto &[id, sock] : clients)
    {
        threads.emplace_back([&, id, sock]()
                             {
            try
            {
                Protocol::sendMessage(sock, 2, "Weights");

                std::vector<float> weights = Protocol::receiveWeights(sock);

                {
                    std::lock_guard<std::mutex> lock(weightsMutex);
                    weightsList.push_back(weights);
                    sizesList.push_back(weights.size());
                }
            }
            catch (...)
            {
                std::cout << "[ERROR] Client " << id << " failed during aggregation\n";
                removeClient(id);
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
        std::lock_guard<std::mutex> lock(statesMutex);
        for (auto &[id, state] : statesMap)
            state = 1; // TRAINING
    }
    aggregationStarted = false;
    startTraining();
}

void Server::removeClient(uint32_t id)
{
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clientMap.erase(id);
    }

    {
        std::lock_guard<std::mutex> lock(statesMutex);
        statesMap.erase(id);
    }
}

bool Server::verifyArchitecture(const Architecture &clientArch)
{
    Architecture arch = globalModel.getArchitecture();
    return clientArch.equals(arch);
}
