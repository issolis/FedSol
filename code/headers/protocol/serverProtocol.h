#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#include "federated/sharedState.h"
#include "logger/logger.h"
#include "security/AuthManager.h"
#include "protocol/protocol.h"
#include <string>
#include <models/Architecture.h>
#include "federated/sharedState.h"
class ServerProtocol
{
private:
    SharedState& shared; 
    AuthManager authManager; 
public:
    ServerProtocol(SharedState& shared);

    void HandShake(std::string &password, int clientSockID, const Architecture &global_arch); 
    static void sendStartTraining(int sock, const std::vector<float>& weights); 
    static void sendDataset(const std::string& datasetPath, SharedState& shared); 
    void HandShakePT(std::string &password, int clientSockID, const std::string &globalHash);
};


#endif