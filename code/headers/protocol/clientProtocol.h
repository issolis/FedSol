#ifndef CLIENTPROTOCOL_H
#define CLIENTPROTOCOL_H

#include "logger/logger.h"
#include "models/Model.h"
#include "protocol/protocol.h"

class ClientProtocol
{
private:
    /* data */
public:
    static int connectAndAuthenticate(const std::string& password, Model& model, const std::string& serverIP, uint16_t port);
    static bool HandShake(int sockID, const std::string& password, Model& model,  uint32_t id); 

    static int connectAndAuthenticatePT(const std::string& password, Model& model, const std::string& serverIP, uint16_t port, const std::string& ptHash);
    static bool HandShakePT(int sockID, const std::string& password, Model& model, uint32_t id, const std::string& ptHash);
};


#endif