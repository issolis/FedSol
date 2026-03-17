#include "network/client.h"
#include <utility>
#include "serializer/modelSerializer.h"
#include "network/connection.h"

Client::Client(unsigned short port, std::string serverIP, std::string password, Model &model)
{
    this->port = port;
    this->serverIP = serverIP;
    this->password = password;
    this->model = model;
}

void Client::sendModel()
{
    int sockID = Connection::createConnection(serverIP, port);
    std::string content = "initialConnection";

    Connection::sendMessage(sockID, 1, content, password, false);

    std::pair<std::vector<char>, std::vector<char>> serializedModel = ModelSerializer::serialize(model);

    std::vector<char> modelBuffer = serializedModel.second;
    std::vector<char> posBuffer = serializedModel.first;

    uint32_t posBufferSize = htonl(posBuffer.size());

    sendAll(sockID, &posBufferSize, sizeof(posBufferSize));
    sendAll(sockID, posBuffer.data(), posBuffer.size());

    uint32_t modelBufferSize = htonl(modelBuffer.size());

    sendAll(sockID, &modelBufferSize, sizeof(modelBufferSize));
    sendAll(sockID, modelBuffer.data(), modelBuffer.size());

    // Response
    uint32_t resp_size;
    recvAll(sockID, &resp_size, sizeof(resp_size));
    resp_size = ntohl(resp_size);

    std::vector<char> resp_buffer(resp_size);

    recvAll(sockID, resp_buffer.data(), resp_size);

    std::string response(resp_buffer.begin(), resp_buffer.end());

    std::cout << "Server response: " << response << std::endl;

    close(sockID);
}

void Client::listener()
{
    int sockID = Connection::createConnection(serverIP, port);

    if (!authenticate(sockID, password, model.getID()))
    {
        std::cout << "Login failed\n";
        close(sockID);
        return;
    }
    Serializer serializer;

    while (true)
    {
        uint32_t bufferSize;

        recvAll(sockID, &bufferSize, sizeof(bufferSize));
        bufferSize = ntohl(bufferSize);
        std::vector<char> buffer(bufferSize);
        recvAll(sockID, buffer.data(), bufferSize);
        Message message = serializer.deserializeMessage(buffer);

        std::cout << "[CLIENT] Command received: "
                  << message.code << " content: " << message.password << std::endl;

        switch (message.code)
        {
            //
        }
    }
}

void Client::setModel(Model &model)
{
    this->model = model;
}

void Client::sendID(int sockID)
{
    auto buffer = Serializer::serializeID(model.getID());
    sendAll(sockID, buffer.data(), buffer.size());
}

bool Client::authenticate(int sockID, const std::string &password, uint32_t id)
{
    std::string content = "Auth";

    Connection::sendMessage(sockID, 0, content, password, false);

    uint32_t resp_size;
    recvAll(sockID, &resp_size, sizeof(resp_size));
    resp_size = ntohl(resp_size);

    std::vector<char> resp_buffer(resp_size);
    recvAll(sockID, resp_buffer.data(), resp_size);

    std::string response(resp_buffer.begin(), resp_buffer.end());

    std::cout << "Server response: " << response << std::endl;

    if (response == "AUTH_SUCCESSFUL")
    {
        sendID(sockID);
        sendArch(sockID);

        uint32_t arch_resp_size;
        recvAll(sockID, &arch_resp_size, sizeof(arch_resp_size));
        arch_resp_size = ntohl(arch_resp_size);

        std::vector<char> arch_resp_buffer(arch_resp_size);
        recvAll(sockID, arch_resp_buffer.data(), arch_resp_size);

        std::string arch_response(arch_resp_buffer.begin(),
                                  arch_resp_buffer.end());

        if (arch_response == "ARCH_OK")
        {
            std::cout << "Architecture accepted\n";
            return true;
        }

        if (arch_response == "ARCH_INVALID")
        {
            std::cout << "Architecture rejected by server\n";
            return false;
        }

        std::cout << "Unknown architecture response\n";
        return false;
    }   

    if (response == "AUTH_FAILED")
    {
        std::cout << "Authentication rejected by server\n";
        return false;
    }

    std::cout << "Unknown server response\n";
    return false;
}

void Client::sendArch(int sockID)
{
    std::vector<char> buffer = Serializer::serializeArchitecture(model.getArchitecture());
    uint32_t bufferSize = htonl(buffer.size());

    sendAll(sockID, &bufferSize, sizeof(uint32_t));
    sendAll(sockID, buffer.data(), buffer.size());
}