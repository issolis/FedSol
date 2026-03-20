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
#include "network/sharedState.h"
#include "network/FLcoordinator.h"
#include "logger/logger.h"

class Server
{
private:
    int server_fd;
    Serializer serializer;
    FLCoordinator coordinator; 
    std::mutex clientsMutex;
    std::mutex statesMutex;
    std::mutex modelMutex;
    Model &globalModel;
    AuthManager authManager;
    SharedState shared; 
    uint32_t epochs = 0; 
    std::string path; 
    

    void handleClient(int clientSockID);
    void removeClient(uint32_t id);
    void authenticate(std::string &password, int sockID);
    void handleMessage(AuthMessage& message, int clientSockID); 

    
    bool verifyArchitecture(const Architecture &clientArch);

public:
    Server(unsigned short port, uint32_t backlog, Model &globaModel, const std::string &path);
    void run();
    void consoleLoop();
};

#endif