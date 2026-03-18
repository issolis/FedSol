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
#include "network/net_utils.h"
#include "protocol/protocol.h"
#include <thread>
#include <mutex>

class Client{
    private:
        unsigned short port;
        std::string serverIP; 
        std::string password; 
        Model model; 
        int state = 0; 
        std::mutex modelMutex; 

    public: 
        Client(unsigned short port, std::string serverIP, std::string password, Model &model); 
        void listener(); 
        void setModel(Model &model); 
        bool authenticate(int sockID, const std::string& password, uint32_t id); 
        int getState(); 
        void reportTrainingEnded(); 
        void run(); 

}; 


#endif