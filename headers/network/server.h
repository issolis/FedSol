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
#include "FedAvg/FedAvg.h"
#include <algorithm>
#include "network/connection.h"

class Server
{
private:
    int server_fd;
    Serializer serializer;

    std::unordered_map<uint32_t, int> clientMap;
    std::unordered_map<uint32_t, int> statesMap;
    std::atomic<bool> aggregationStarted = false;
    std::mutex clientsMutex;
    std::mutex statesMutex;
    std::mutex modelMutex;
    Model globalModel;
    AuthManager authManager;
    

    void handleClient(int clientSockID);
    void removeClient(uint32_t id);
    void authenticate(std::string &password, int sockID);
    void startTraining();
    void aggregate();
    void handleMessage(AuthMessage& message, int clientSockID); 

    
    bool verifyArchitecture(const Architecture &clientArch);

public:
    Server(unsigned short port, uint32_t backlog, Model &globaModel);
    void run();
    void consoleLoop();
};

#endif