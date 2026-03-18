#include "network/connection.h"
#include <arpa/inet.h>
#include <sys/socket.h>
Connection::Connection()
{
}

int Connection::createConnection(const std::string &serverIP, uint16_t port)
{
    int sockID = socket(AF_INET, SOCK_STREAM, 0);

    if (sockID < 0)
        throw std::runtime_error("Socket creation error");

    sockaddr_in server_addr{};
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP.c_str(), &server_addr.sin_addr) <= 0)
        throw std::runtime_error("Invalid address");

    if (connect(sockID, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        throw std::runtime_error("Connection failed");

    return sockID;
}

int Connection::createServerConnection(int backlog, unsigned short port){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_fd < 0)
    {
        std::cerr << "Socket creation failed\n";
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Error binding socket\n";
        exit(EXIT_FAILURE);
    }

    listen(server_fd, backlog);
    return server_fd; 
}

