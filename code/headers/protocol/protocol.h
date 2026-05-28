#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include "models/Model.h"
#include "serializer/serializer.h"
#include "network/net_utils.h"
#include <fstream>
#include <logger/logger.h>

class Protocol
{
public:
    static void sendID(int sock, uint32_t id);
    static uint32_t receiveID(int sock);

    static void sendArchitecture(int sock, const Architecture &arch);
    static Architecture receiveArchitecture(int sock);

    static void sendModel(int sock, const Model &model);
    static Model receiveModel(int sock);

    static void sendWeights(int sock, const std::vector<float> &weights);
    static std::vector<float> receiveWeights(int sock);

    static void sendAuthMessage(int sockID, int code, const std::string &password, const std::string &content);
    static AuthMessage receiveAuthMessage(int sockID);

    static void sendMessage(int sockID, int code, const std::string &content);
    static Message receiveMessage(int sockID);

    static void sendSamplesSize(int sockID, uint32_t samplesSize);
    static uint32_t receiveSamplesSize(int sockID);

    static void sendFile(int sockID, const std::string &filepath);
    static void receiveFile(int sockID, const std::string &outputPath);

    static void sendString(int sockID, const std::string &str);
    static std::string receiveString(int sockID);

    static void sendError(int sockID, const std::string &origin, const std::string &errorMsg, const std::string &password);
    static std::string receiveError(int sockID);

    static void sendState(int sockID, const std::string &state);
    static std::string receiveState(int sockID);

    static void sendHash(int sockID, const std::string &hash);
    static std::string receiveHash(int sockID);
};

#endif