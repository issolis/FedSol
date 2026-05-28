#include "network/server.h"
#include "network/serverConsoleManager.h"
#include "protocol/OP_CODES.h"

Server::Server(
    unsigned short port,
    uint32_t backlog,
    Model &globalModel,
    const std::string &path,
    const std::string &datasetPath,
    bool defenseEnabled,
    const std::string &ptHash)
    : globalModel(globalModel),
      trainer(shared, globalModel),
      aggregator(shared, globalModel),
      serverProtocol(shared),
      roundManager(shared, trainer, aggregator),
      watchdog(shared, roundManager)
{
    Logger::init("output_server/logs/server.log");
    server_fd = Connection::createServerConnection(backlog, port);
    Logger::log(LogLevel::INFO, "Client starting...");
    serializer = Serializer();
    this->path = path;
    this->datasetPath = datasetPath;
    this->ptHash = ptHash;

    shared.defenseEnabled.store(defenseEnabled); // ← AÑADIDO
    Logger::log(LogLevel::INFO,
                std::string("[Server] Backdoor defense is ") +
                    (defenseEnabled ? "ENABLED" : "DISABLED") + ".");
}

void Server::run()
{
    Logger::log(LogLevel::INFO, "[MODE] INITIALIZATION");
    watchdog.start();

    consoleThread = std::thread(&Server::consoleLoop, this);

    while (running.load())
    {
        int clientSock = accept(server_fd, nullptr, nullptr);
        if (clientSock < 0)
            continue;

        std::lock_guard<std::mutex> lock(threadsMutex);
        clientThreads.emplace_back(&Server::handleClient, this, clientSock);
    }

    // Cleanup: join all client threads
    {
        std::lock_guard<std::mutex> lock(threadsMutex);
        for (auto &t : clientThreads)
        {
            if (t.joinable())
                t.join();
        }
    }

    if (consoleThread.joinable())
        consoleThread.join();
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
        Logger::log(LogLevel::ERROR, "[Server] Client error: " + std::string(e.what()));
    }
}

void Server::handleMessage(AuthMessage &message, int clientSockID)
{
    switch (message.code)
    {
    case AuthOp::HANDSHAKE:
    {
        if (shared.trainingActive.load())
        {
            Logger::log(LogLevel::WARNING,
                        "[Server] New client rejected — training in progress.");
            Protocol::sendMessage(clientSockID, ServerOp::AUTH_RESPONSE, "TRAINING_IN_PROGRESS");
            close(clientSockID);
            break;
        }

        if (!this->ptHash.empty())
        {
            // pt_path mode: validate by .pt hash
            serverProtocol.HandShakePT(message.password, clientSockID, this->ptHash);
        }
        else
        {
            // Normal mode: validate by architecture
            serverProtocol.HandShake(message.password, clientSockID, this->globalModel.getArchitecture());
        }
        break;
    }

    case AuthOp::TRAINING_FINISHED:
    {
        roundManager.handleTrainingFinished(clientSockID, path);
        break;
    }

    case AuthOp::ERROR:
    {
        std::string error = Protocol::receiveError(clientSockID);
        Logger::log(LogLevel::ERROR, "[Server] Client error: " + error);
        close(clientSockID);
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

void Server::consoleLoop()
{

    std::cout << "Operator password: ";
    std::string pwd;
    std::getline(std::cin, pwd);

    if (!authManager.authenticate(pwd))
    {
        Logger::log(LogLevel::ERROR, "[Console] Unauthorized operator. Access denied.");
        std::cout << "[ERROR] Access denied." << std::endl;
        return;
    }

    Logger::log(LogLevel::INFO, "[Console] Operator authenticated.");

    while (true)
    {
        int option = ServerConsoleManager::menu();

        switch (option)
        {
        case 1:
        {
            ServerConsoleManager::handleStartTraining(this->globalModel, shared, trainer, this->ptHash);
            break;
        }

        case 2:
        {
            ServerConsoleManager::handleShowClients(this->shared);
            break;
        }

        case 3:
        {
            ServerConsoleManager::handlePopulateModel(this->globalModel, this->path);
            break;
        }
        case 4:
        {
            ServerConsoleManager::handleEvaluation(this->path, "evaluate");
            break;
        }
        case 5:
        {
            ServerConsoleManager::handleSplitDataset(datasetPath, shared);
            break;
        }
        case 6:

            ServerConsoleManager::handleStopClients(this->shared);
            break;
        case 7:
        {
            char confirm;
            std::cout << "Are you sure you want to exit? (y/n): ";
            std::cin >> confirm;

            if (confirm == 'y' || confirm == 'Y')
            {
                watchdog.stop();
                Logger::log(LogLevel::INFO, "[SERVER] Shutting down.");
                std::cout << "[INFO] Server shutting down..." << std::endl;
                running = false;
                close(server_fd); // desbloquea el accept()
                return;           // sale del consoleLoop, join lo recoge
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
