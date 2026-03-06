#include "network/server.h"
#include "models/Model.h"
Server::Server(unsigned short port, uint32_t backlog)
{
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

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

        uint32_t size;

        recvAll(clientSockID, &size, sizeof(size));
        size = ntohl(size);

        std::vector<char> buffer(size);

        std::cout << "Bytes Received " << size << std::endl;

        recvAll(clientSockID, buffer.data(), size);

        Model localmodel = Model(); 

        localmodel.deserializePos(buffer); 
        std::cout<<localmodel.getNonSerializedPos()[0]<<std::endl; 
        std::cout<<localmodel.getNonSerializedPos()[1]<<std::endl; 
        std::cout<<localmodel.getNonSerializedPos()[2]<<std::endl; 
        std::cout<<localmodel.getNonSerializedPos()[3]<<std::endl; 

        std::cout << "Received Bytes:\n";
        printBytes(buffer.data(), buffer.size());

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

    close(clientSockID);
}
