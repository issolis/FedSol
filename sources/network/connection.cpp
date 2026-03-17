#include "network/connection.h"

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

