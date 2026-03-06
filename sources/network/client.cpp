#include "network/client.h"

Client::Client(unsigned short port, std::string serverIP)
{

    sockID = socket(AF_INET, SOCK_STREAM, 0);

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
}

void Client::send(std::vector<char> &buffer)
{
    uint32_t size = htonl(buffer.size());

    sendAll(sockID, &size, sizeof(size));
    sendAll(sockID, buffer.data(), buffer.size());

    std::cout << "Sent Bytes:\n";
    printBytes(buffer.data(), buffer.size());

    uint32_t resp_size;

    recvAll(sockID, &resp_size, sizeof(resp_size));
    resp_size = ntohl(resp_size);

    std::vector<char> resp_buffer(resp_size);

    recvAll(sockID, resp_buffer.data(), resp_size);

    std::string response(resp_buffer.begin(), resp_buffer.end());

    std::cout << "Server response: " << response << std::endl;

    close(sockID);
}