#ifndef SERVER_H

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

class Server
{
private:
    int server_fd;
    sockaddr_in address{};
    Serializer serializer;
    void handleClient(int clientSockID);
    void authenticate(std::string &password, int sockID);
    Model globalModel;
    std::unordered_map<uint32_t, int> clientMap;
    AuthManager authManager;
    bool verifyArchitecture(const Architecture &clientArch);

public:
    Server(unsigned short port, uint32_t backlog, Model &globaModel);
    void run();
};

#endif