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
#include <atomic>
#include <jsonManager/jsonManager.h>

enum class ClientState { IDLE, TRAINING };

class Client{
    private:
        unsigned short port;
        std::string serverIP; 
        std::string password; 
        std::string path; 
        std::string datasetPath; 
        Model &model; 
        std::mutex modelMutex; 
        std::string ptHash; 
        std::atomic<ClientState> state{ClientState::IDLE}; 

    public: 
        Client(unsigned short port, std::string serverIP, std::string password, Model &model, const std::string &path, const std::string &datasetPath, const std::string &ptHash = ""); 
        void listener(); 
        void run();     
        void startLocalTraining(int sockID);
        void sendWeights(int sockID); 
        void receiveDataset(int sockID); 
        void sendState(int sockID); 

}; 


#endif