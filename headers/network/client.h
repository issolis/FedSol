#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <vector>
#include <cstring>
#include <serializer/serializer.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include "models/Model.h"
#include "net_utils.h"


class Client{
    private:
        unsigned short port;
        std::string serverIP; 
        std::string password; 
        Model model; 

    public: 
        Client(unsigned short port, std::string serverIP, std::string password, Model &model); 
        void sendModel(); 
        void listener(); 
        void setModel(Model &model); 
        void sendID(int sockID);
        void sendArch(int sockID);
        bool authenticate(int sockID, const std::string& password, uint32_t id); 

}; 


#endif