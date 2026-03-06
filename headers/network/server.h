#ifndef SERVER_H

#include <iostream>
#include <thread>
#include <vector>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network/net_utils.h"


class Server{
private:
    int server_fd;
    sockaddr_in address{};

public:
    Server(unsigned short port, uint32_t backlog);
    void run();
    void handleClient(int clientSockID); 
};

#endif