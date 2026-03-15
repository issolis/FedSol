#include "network/client.h"
#include <utility>
#include "serializer/modelSerializer.h"
#include "network/connection.h"

Client::Client(unsigned short port, std::string serverIP, std::string password)
{
    this->port = port;
    this->serverIP = serverIP;
    this->password =  password;
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
    std::string content = "initialConnection"; 
    Connection::sendMessage(sockID, 0, content, password, false);
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
        Message message= serializer.deserializeMessage(buffer);

        std::cout << "[CLIENT] Command received: "
                  << message.code << " content: " << message.password << std::endl;

        switch (message.code)
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