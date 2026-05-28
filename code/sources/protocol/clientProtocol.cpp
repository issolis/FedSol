#include "protocol/clientProtocol.h"
#include "network/connection.h"
#include <string>
#include "protocol/OP_CODES.h"


int ClientProtocol::connectAndAuthenticate(const std::string& password, Model& model, const std::string& serverIP, uint16_t port)
{
    int sockID = Connection::createConnection(serverIP, port);

    if (sockID < 0)
    {
        throw std::runtime_error("Failed to create connection");
    }

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Connected to server");

    if (!ClientProtocol::HandShake(sockID, password, model, model.getID()))
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(sockID) + "] Authentication failed");

        close(sockID);

        throw std::runtime_error("Authentication failed");
    }

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Authenticated successfully");

    return sockID;
}

int ClientProtocol::connectAndAuthenticatePT(const std::string& password, Model& model, const std::string& serverIP, uint16_t port, const std::string& ptHash)
{
    int sockID = Connection::createConnection(serverIP, port);

    if (sockID < 0)
        throw std::runtime_error("Failed to create connection");

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Connected to server (pt_path mode)");

    if (!ClientProtocol::HandShakePT(sockID, password, model, model.getID(), ptHash))
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(sockID) + "] PT handshake failed");

        close(sockID);
        throw std::runtime_error("PT handshake failed");
    }

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Authenticated successfully (pt_path mode)");

    return sockID;
}

bool ClientProtocol::HandShakePT(int sockID, const std::string& password, Model& model, uint32_t id, const std::string& ptHash)
{
    Protocol::sendAuthMessage(sockID, AuthOp::HANDSHAKE, password, "Auth");

    Message msg = Protocol::receiveMessage(sockID);

    if (msg.content != "AUTH_SUCCESSFUL")
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(sockID) + "] Authentication rejected by server");
        return false;
    }

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Authentication successful (pt_path mode)");

    Protocol::sendID(sockID, model.getID());

    Logger::log(LogLevel::DEBUG,
                "[Client " + std::to_string(id) + "] Sent client ID");

    Protocol::sendHash(sockID, ptHash);

    Logger::log(LogLevel::DEBUG,
                "[Client " + std::to_string(id) + "] Sent .pt hash: " + ptHash.substr(0, 8) + "...");

    Message resp = Protocol::receiveMessage(sockID);

    Logger::log(LogLevel::DEBUG,
                "[Client " + std::to_string(id) + "] Hash response: " + resp.content);

    if (resp.content == "HASH_OK")
    {
        Logger::log(LogLevel::INFO,
                    "[Client " + std::to_string(id) + "] PT hash verified");
        return true;
    }
    else
    {
        Logger::log(LogLevel::ERROR,
                    "[Client " + std::to_string(id) + "] PT hash rejected by server: " + resp.content);
        return false;
    }
}

bool ClientProtocol::HandShake(int sockID, const std::string& password, Model& model,  uint32_t id){
    std::string content = "Auth";

    Logger::log(LogLevel::INFO,
                "[Sock " + std::to_string(sockID) + "] Sending authentication request");

    Protocol::sendAuthMessage(sockID, AuthOp::HANDSHAKE, password, content);

    Message msg = Protocol::receiveMessage(sockID);

    Logger::log(LogLevel::DEBUG,
                "[Sock " + std::to_string(sockID) + "] Server response: " + msg.content);

    if (msg.content == "AUTH_SUCCESSFUL")
    {
        Logger::log(LogLevel::INFO,
                    "[Sock " + std::to_string(sockID) + "] Authentication successful");

        Protocol::sendID(sockID, model.getID());

        Logger::log(LogLevel::DEBUG,
                    "[Client " + std::to_string(id) + "] Sent client ID");

        Protocol::sendArchitecture(sockID, model.getArchitecture());

        Logger::log(LogLevel::DEBUG,
                    "[Client " + std::to_string(id) + "] Sent architecture");

        Message msg = Protocol::receiveMessage(sockID);

        Logger::log(LogLevel::DEBUG,
                    "[Client " + std::to_string(id) + "] Architecture response: " + msg.content);

        if (msg.content == "ARCH_OK")
        {
            Logger::log(LogLevel::INFO,
                        "[Client " + std::to_string(id) + "] Architecture verified");

            return true;
        }
        else
        {
            Logger::log(LogLevel::ERROR,
                        "[Client " + std::to_string(id) + "] Architecture rejected by server");

            return false;
        }

        Logger::log(LogLevel::WARNING,
                    "[Client " + std::to_string(id) + "] Unknown architecture response");

        return false;
    }
    else
    {
        Logger::log(LogLevel::ERROR,
                    "[Sock " + std::to_string(sockID) + "] Authentication rejected by server");

        return false;
    }

    Logger::log(LogLevel::WARNING,
                "[Sock " + std::to_string(sockID) + "] Unknown server response");

    return false;
}