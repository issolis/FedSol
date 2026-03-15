#ifndef SERVER_H

#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include "serializer/serializer.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network/net_utils.h"


class Server{
private:
    int server_fd;
    sockaddr_in address{};
    Serializer serializer; 
    void handleClient(int clientSockID); 
    void receiveModel(int sockID); 
    uint32_t receiveID(int clientSockID);


public:
    Server(unsigned short port, uint32_t backlog);
    void run();
    
};

#endif