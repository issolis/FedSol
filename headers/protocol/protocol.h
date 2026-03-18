#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include "models/Model.h"
#include "serializer/serializer.h"
#include "network/net_utils.h"

class Protocol
{
public:
    static void sendID(int sock, uint32_t id);
    static uint32_t receiveID(int sock);

    static void sendArchitecture(int sock, const Architecture& arch);
    static Architecture receiveArchitecture(int sock);

    static void sendModel(int sock, const Model& model);
    static Model receiveModel(int sock);

    static void sendWeights(int sock, const std::vector<float>& weights);
    static std::vector<float> receiveWeights(int sock);

    static void sendAuthMessage(int sockID, int code, const std::string& password, const std::string& content);
    static AuthMessage receiveAuthMessage(int sockID);

    static void sendMessage(int sockID, int code, const std::string& content);
    static Message receiveMessage(int sockID);


};

#endif