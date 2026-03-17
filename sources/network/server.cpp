#include "network/server.h"
#include "models/Model.h"
#include "protocol/protocol.h"

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


        AuthMessage message = Protocol::receiveAuthMessage(clientSockID); 

        std::cout << "[SERVER] Code: " << message.code << std::endl;
        std::cout << "[SERVER] Password: " << message.content << std::endl;

        switch (message.code)
        {
        case 0:
        {
            authenticate(message.password, clientSockID);
            break;
        }

        case 1:
        {
            //receiveModel(clientSockID);
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

void Server::authenticate(std::string &password, int clientSockID)
{
    std::string response;

    if (!authManager.authenticate(password))
    {
        std::cout << "[SERVER] Authentication failed\n";

        response = "AUTH_FAILED";

        Protocol::sendMessage(clientSockID, 0, response); 
        close(clientSockID);
        return;
    }

    std::cout << "[SERVER] Authentication success\n";
    response = "AUTH_SUCCESSFUL";
    Protocol::sendMessage(clientSockID, 0, response); 
    uint32_t id = Protocol::receiveID(clientSockID);
    Architecture clientArch = Protocol::receiveArchitecture(clientSockID);

    if (!verifyArchitecture(clientArch))
    {
        std::cout << "[SERVER] Architecture mismatch\n";

        std::string resp = "ARCH_INVALID";
        Protocol::sendMessage(clientSockID, 0, resp); 

        close(clientSockID);
        return;
    }

    clientMap[id] = clientSockID;
    std::cout << "[SERVER] Client registered with id: " << id << std::endl;
    std::cout << "[SERVER] Architecture verified\n";

    std::string resp = "ARCH_OK";
    Protocol::sendMessage(clientSockID, 0, resp); 
}

bool Server::verifyArchitecture(const Architecture &clientArch)
{
    Architecture arch = globalModel.getArchitecture();
    return clientArch.equals(arch);
}