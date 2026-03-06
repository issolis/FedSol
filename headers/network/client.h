#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <vector>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net_utils.h"

class Client{
    private:
        int sockID; 
        sockaddr_in server_addr{};

    public: 
        Client(unsigned short port, std::string serverIP); 
        void send(std::vector<char> &buffer); 

}; 


#endif