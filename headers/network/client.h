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
        void sendMessage(int sockID, int code, std::string content, bool closeConnection); 
        Model model; 

    public: 
        Client(unsigned short port, std::string serverIP); 
        void sendModel(); 
        void listener(); 
        void setModel(Model &model); 
        void sendID(int sockID);
        int getNewConnection();  



}; 


#endif