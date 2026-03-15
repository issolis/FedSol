#include "network/client.h"
#include <utility>
#include "serializer/modelSerializer.h"

Client::Client(unsigned short port, std::string serverIP)
{
    this->port = port;
    this->serverIP = serverIP;
}

void Client::sendModel()
{
    int sockID = getNewConnection();

    sendMessage(sockID, 1, "sending model", false);

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

int Client::getNewConnection()
{
    int sockID = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};

    if (sockID < 0)
    {
        std::cerr << "Socket creation error\n";
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP.c_str(), &server_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address\n";
        exit(EXIT_FAILURE);
    }

    if (connect(sockID, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Connection failed\n";
        exit(EXIT_FAILURE);
    }

    return sockID;
}

void Client::sendMessage(int sockID, int code, std::string content, bool closeConnection)
{

    Serializer serializer = Serializer();
    std::vector<char> buffer = serializer.serializeMessage(code, content);

    uint32_t bufferSize = htonl(buffer.size());

    sendAll(sockID, &bufferSize, sizeof(bufferSize));
    sendAll(sockID, buffer.data(), buffer.size());

    if (closeConnection)
        close(sockID);
}

void Client::listener()
{
    int sockID = getNewConnection();
    sendMessage(sockID, 0, "initialConnection", false);
    sendID(sockID);

    sendModel(); 
    Serializer serializer;

    while (true)
    {
        uint32_t bufferSize;

        recvAll(sockID, &bufferSize, sizeof(bufferSize));
        bufferSize = ntohl(bufferSize);
        std::vector<char> buffer(bufferSize);
        recvAll(sockID, buffer.data(), bufferSize);
        auto [code, content] = serializer.deserializeMessage(buffer);

        std::cout << "[CLIENT] Command received: "
                  << code << " content: " << content << std::endl;

        switch (code)
        {
            //
        }
    }
}

void Client::setModel(Model& model)
{
    this->model = model;
}

void Client::sendID(int sockID)
{
    auto buffer = Serializer::serializeID(model.getID());
    sendAll(sockID, buffer.data(), buffer.size());
}