#include "network/server.h"
#include "models/Model.h"
#include "serializer/modelSerializer.h"

Server::Server(unsigned short port, uint32_t backlog, Model &globalModel)
{
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    this->globalModel = globalModel;
    if (server_fd < 0)
    {
        std::cerr << "Socket creation failed\n";
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Error binding socket\n";
        exit(EXIT_FAILURE);
    }

    listen(server_fd, backlog);

    std::cout << "Server listening on port " << port << std::endl;

    serializer = Serializer();
}

void Server::run()
{
    while (true)
    {
        int clientSock = accept(server_fd, nullptr, nullptr);
        if (clientSock < 0)
            continue;

        std::thread(&Server::handleClient, this, clientSock).detach();
    }
}

void Server::handleClient(int clientSockID)
{

    try
    {
        std::cout << "Client connected: " << clientSockID << std::endl;

        uint32_t bufferSize;
        recvAll(clientSockID, &bufferSize, sizeof(bufferSize));
        bufferSize = ntohl(bufferSize);

        std::vector<char> buffer(bufferSize);
        recvAll(clientSockID, buffer.data(), bufferSize);

        Message message = serializer.deserializeMessage(buffer);

        std::cout << "[SERVER] Code: " << message.code << std::endl;
        std::cout << "[SERVER] Content: " << message.content << std::endl;

        switch (message.code)
        {
        case 0:
        {
            authenticate(message.password, clientSockID);
            break;
        }

        case 1:
        {
            receiveModel(clientSockID);
            close(clientSockID);
            break;
        }

        default:
            std::cout << "[SERVER] Unknown message code" << std::endl;
            break;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

void Server::receiveModel(int clientSockID)
{
    try
    {

        uint32_t posBufferSize;

        recvAll(clientSockID, &posBufferSize, sizeof(posBufferSize));
        posBufferSize = ntohl(posBufferSize);

        std::vector<char> posBuffer(posBufferSize);
        recvAll(clientSockID, posBuffer.data(), posBufferSize);

        uint32_t modelBufferSize;

        recvAll(clientSockID, &modelBufferSize, sizeof(modelBufferSize));
        modelBufferSize = ntohl(modelBufferSize);

        std::vector<char> modelBuffer(modelBufferSize);
        recvAll(clientSockID, modelBuffer.data(), modelBufferSize);

        Model modelReceived = ModelSerializer::deserialize(posBuffer, modelBuffer);

        std::cout << modelReceived.getID() << std::endl;
        std::string response = "Message received";

        uint32_t resp_size = htonl(response.size());

        sendAll(clientSockID, &resp_size, sizeof(resp_size));
        sendAll(clientSockID, response.data(), response.size());
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "Client disconnected: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown client error\n";
    }
}

void Server::authenticate(std::string &password, int clientSockID)
{
    std::string response;

    if (!authManager.authenticate(password))
    {
        std::cout << "[SERVER] Authentication failed\n";

        response = "AUTH_FAILED";

        uint32_t resp_size = htonl(response.size());
        sendAll(clientSockID, &resp_size, sizeof(resp_size));
        sendAll(clientSockID, response.data(), response.size());

        close(clientSockID);
        return;
    }

    std::cout << "[SERVER] Authentication success\n";
    response = "AUTH_SUCCESSFUL";
    uint32_t resp_size = htonl(response.size());
    sendAll(clientSockID, &resp_size, sizeof(resp_size));
    sendAll(clientSockID, response.data(), response.size());
    uint32_t id = receiveID(clientSockID);

    Architecture clientArch = receiveArch(clientSockID);

    if (!verifyArchitecture(clientArch))
    {
        std::cout << "[SERVER] Architecture mismatch\n";

        std::string resp = "ARCH_INVALID";
        uint32_t size = htonl(resp.size());

        sendAll(clientSockID, &size, sizeof(size));
        sendAll(clientSockID, resp.data(), resp.size());

        close(clientSockID);
        return;
    }

    clientMap[id] = clientSockID;
    std::cout << "[SERVER] Client registered with id: " << id << std::endl;
    std::cout << "[SERVER] Architecture verified\n";

    std::string resp = "ARCH_OK";
    uint32_t size = htonl(resp.size());

    sendAll(clientSockID, &size, sizeof(size));
    sendAll(clientSockID, resp.data(), resp.size());
}

uint32_t Server::receiveID(int sockID)
{
    std::vector<char> buffer(sizeof(uint32_t));
    recvAll(sockID, buffer.data(), sizeof(uint32_t));
    uint32_t nodeID = Serializer::deserializeID(buffer);
    return nodeID;
}

Architecture Server::receiveArch(int sockID)
{
    uint32_t archSize;

    recvAll(sockID, &archSize, sizeof(archSize));
    archSize = ntohl(archSize);

    std::cout << "Receiving architecture size: "
              << archSize << std::endl;

    std::vector<char> buffer(archSize);
    recvAll(sockID, buffer.data(), archSize);

    Architecture arch =
        Serializer::deserializeArchitecture(buffer);

    return arch;
}

bool Server::verifyArchitecture(const Architecture &clientArch)
{
    Architecture arch = globalModel.getArchitecture();
    return clientArch.equals(arch);
}