#ifndef SERVER_H
#include <mutex>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include "serializer/serializer.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "models/Model.h"
#include "security/AuthManager.h"
#include <unordered_map>
#include "network/net_utils.h"
#include <atomic>
#include "protocol/protocol.h"
#include "federated/FedAvg.h"
#include <algorithm>
#include "network/connection.h"
#include "federated/sharedState.h"
#include "logger/logger.h"
#include "federated/aggregator.h"
#include "weightsUtils/weightsUtils.h"
#include <thread>
#include <chrono>
#include "jsonManager/jsonManager.h"
#include "protocol/serverProtocol.h"
#include "federated/roundManager.h"
#include "federated/watchdog.h"

class Server
{
private:
    int server_fd;
    Serializer serializer;
    std::mutex clientsMutex;
    std::mutex statesMutex;
    std::mutex modelMutex;
    Trainer trainer;
    Aggregator aggregator;
    Model &globalModel;
    AuthManager authManager;
    SharedState shared;
    std::string path;
    ServerProtocol serverProtocol;
    RoundManager roundManager;
    Watchdog watchdog;
    std::string datasetPath;
    std::vector<std::thread> clientThreads;
    std::mutex threadsMutex;
    std::atomic<bool> running{true};
    std::thread consoleThread;
    std::string ptHash; 
    
    void handleClient(int clientSockID);
    void handleMessage(AuthMessage &message, int clientSockID);

public:
    Server(unsigned short port, uint32_t backlog, Model &globaModel, const std::string &path, const std::string &datasetPath, const std::string &ptHash = "");
    void run();
    void consoleLoop();
};

#endif