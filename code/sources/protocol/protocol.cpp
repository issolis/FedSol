#include "protocol/protocol.h"
#include <utility>
#include "protocol/OP_CODES.h"

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

void Protocol::sendSamplesSize(int sockID, uint32_t samplesSize)
{
    auto buffer = Serializer::serializeID(samplesSize);
    sendAll(sockID, buffer.data(), buffer.size());
}

void Protocol::sendFile(int sockID, const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        Logger::log(LogLevel::ERROR, "[Protocol] Cannot open file: " + filepath);
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    std::vector<char> buffer(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    Logger::log(LogLevel::INFO, "[Protocol] Sending file: " + filepath + 
                " (" + std::to_string(buffer.size()) + " bytes)");

    uint32_t bufferSize = htonl(buffer.size());
    sendAll(sockID, &bufferSize, sizeof(uint32_t));
    sendAll(sockID, buffer.data(), buffer.size());
}

void Protocol::sendString(int sockID, const std::string& str)
{
    uint32_t size = htonl(str.size());
    sendAll(sockID, &size, sizeof(uint32_t));
    sendAll(sockID, str.c_str(), str.size());
}

void Protocol::sendError(int sockID, const std::string& origin, const std::string& errorMsg, const std::string &password)
{
    Protocol::sendAuthMessage(sockID, AuthOp::ERROR, password, "Error" );
    Protocol::sendString(sockID, origin);
    Protocol::sendString(sockID, errorMsg);
}

void Protocol::sendState(int sockID, const std::string& state)
{
    sendString(sockID, state);  
}

std::string Protocol::receiveState(int sockID)
{
    return receiveString(sockID);  
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

    if (weightsSize == 0 || weightsSize > 1024 * 1024 * 1024) // 256 MB máx
    {
        throw std::runtime_error("Invalid weights size: " + std::to_string(weightsSize));
    }

    std::vector<char> buffer(weightsSize);
    recvAll(sockID, buffer.data(), weightsSize);

    return Serializer::deserializeWeights(buffer);
}

Model Protocol::receiveModel(int sockID) {
    uint32_t id = receiveID(sockID);
    Architecture architecture = receiveArchitecture(sockID);
    std::vector<float> weights = receiveWeights(sockID);

    Model model(id, architecture, weights);
    return model; 
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

uint32_t Protocol::receiveSamplesSize(int sockID)
{
    std::vector<char> buffer(sizeof(uint32_t));
    recvAll(sockID, buffer.data(), sizeof(uint32_t));
    uint32_t sampleSize = Serializer::deserializeID(buffer);
    return sampleSize;
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

void Protocol::receiveFile(int sockID, const std::string& outputPath)
{
    uint32_t fileSize;
    recvAll(sockID, &fileSize, sizeof(fileSize));
    fileSize = ntohl(fileSize);

    if (fileSize == 0 || fileSize > 1024 * 1024 * 1024)
    {
        Logger::log(LogLevel::ERROR, "[Protocol] Invalid file size: " + std::to_string(fileSize));
        throw std::runtime_error("Invalid file size: " + std::to_string(fileSize));
    }

    Logger::log(LogLevel::INFO, "[Protocol] Receiving file: " + outputPath +
                " (" + std::to_string(fileSize) + " bytes)");

    std::vector<char> buffer(fileSize);
    recvAll(sockID, buffer.data(), fileSize);

    std::string dir = outputPath.substr(0, outputPath.find_last_of('/'));
    if (!dir.empty())
        system(("mkdir -p " + dir).c_str());

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open())
    {
        Logger::log(LogLevel::ERROR, "[Protocol] Cannot write file: " + outputPath);
        throw std::runtime_error("Cannot write file: " + outputPath);
    }

    out.write(buffer.data(), fileSize);
    Logger::log(LogLevel::INFO, "[Protocol] File saved to: " + outputPath);
}

std::string Protocol::receiveString(int sockID)
{
    uint32_t size;
    recvAll(sockID, &size, sizeof(uint32_t));
    size = ntohl(size);

    std::vector<char> buffer(size);
    recvAll(sockID, buffer.data(), size);

    return std::string(buffer.data(), size);
}

std::string Protocol::receiveError(int sockID)
{
    std::string origin = Protocol::receiveString(sockID);
    std::string errorMsg = Protocol::receiveString(sockID);
    
    std::string fullMsg = "[" + origin + "] " + errorMsg;
    Logger::log(LogLevel::ERROR, "[Protocol] Error received: " + fullMsg);
    return fullMsg;
}

void Protocol::sendHash(int sockID, const std::string &hash)
{
    Protocol::sendString(sockID, hash);
}

std::string Protocol::receiveHash(int sockID)
{
    return Protocol::receiveString(sockID);
}