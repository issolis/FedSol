#include "protocol/protocol.h"

void Protocol::sendArchitecture(int sockID, const Architecture& architecture)
{
    std::vector<char> buffer = Serializer::serializeArchitecture(architecture);
    uint32_t bufferSize = htonl(buffer.size());

    sendAll(sockID, &bufferSize, sizeof(uint32_t));
    sendAll(sockID, buffer.data(), buffer.size());
}

void Protocol::sendID(int sockID, uint32_t id)
{
    auto buffer = Serializer::serializeID(id);
    sendAll(sockID, buffer.data(), buffer.size());
}

void Protocol::sendWeights(int sockID, const std::vector<float>& weights)
{
    std::vector<char> buffer = Serializer::serializeWeights(weights);
    uint32_t bufferSize = htonl(buffer.size());

    sendAll(sockID, &bufferSize, sizeof(uint32_t));
    sendAll(sockID, buffer.data(), buffer.size());
}

void Protocol::sendModel(int sockID, const Model& model){
    sendID(sockID, model.getID()); 
    sendArchitecture(sockID, model.getArchitecture()); 
    sendWeights(sockID, model.getWeights()); 
}

void Protocol::sendAuthMessage(int sockID, int code, const std::string& password, const std::string& content)
{
    std::vector<char> buffer = Serializer::serializeAuthMessage(code, password, content);

    uint32_t bufferSize = htonl(buffer.size());

    sendAll(sockID, &bufferSize, sizeof(bufferSize));
    sendAll(sockID, buffer.data(), buffer.size());
}

void Protocol::sendMessage(int sockID, int code, const std::string& content)
{
    std::vector<char> buffer = Serializer::serializeMessage(code, content);

    uint32_t bufferSize = htonl(buffer.size());

    sendAll(sockID, &bufferSize, sizeof(bufferSize));
    sendAll(sockID, buffer.data(), buffer.size());
}

Architecture Protocol::receiveArchitecture(int sockID)
{
    uint32_t archSize;

    recvAll(sockID, &archSize, sizeof(archSize));
    archSize = ntohl(archSize);

    std::vector<char> buffer(archSize);
    recvAll(sockID, buffer.data(), archSize);

    Architecture arch =
        Serializer::deserializeArchitecture(buffer);

    return arch;
}

uint32_t Protocol::receiveID(int sockID)
{
    std::vector<char> buffer(sizeof(uint32_t));
    recvAll(sockID, buffer.data(), sizeof(uint32_t));
    uint32_t nodeID = Serializer::deserializeID(buffer);
    return nodeID;
}

std::vector<float> Protocol::receiveWeights(int sockID)
{
    uint32_t weightsSize;

    recvAll(sockID, &weightsSize, sizeof(weightsSize));
    weightsSize = ntohl(weightsSize);

    std::vector<char> buffer(weightsSize);
    recvAll(sockID, buffer.data(), weightsSize);

    std::vector<float> weights = Serializer::deserializeWeights(buffer); 
    
    return weights;
}

Model Protocol::receiveModel(int sockID){
    float id = receiveID(sockID); 
    Architecture architecture = receiveArchitecture(sockID); 
    std::vector<float> weights = receiveWeights(sockID); 

    return Model(id, architecture, weights); 
}

AuthMessage Protocol::receiveAuthMessage(int sockID)
{
    uint32_t bufferSize;

    recvAll(sockID, &bufferSize, sizeof(bufferSize));
    bufferSize = ntohl(bufferSize);

    std::vector<char> buffer(bufferSize);
    recvAll(sockID, buffer.data(), bufferSize);

    return Serializer::deserializeAuthMessage(buffer);
}

Message Protocol::receiveMessage(int sockID)
{
    uint32_t bufferSize;

    recvAll(sockID, &bufferSize, sizeof(bufferSize));
    bufferSize = ntohl(bufferSize);
    std::vector<char> buffer(bufferSize);
    recvAll(sockID, buffer.data(), bufferSize);

    return Serializer::deserializeMessage(buffer);
}