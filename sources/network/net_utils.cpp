#include "network/net_utils.h"
#include <iostream>
#include <cerrno>
#include <iomanip>
#include <cstring>
void sendAll(int sockid, const void *data, size_t size)
{
    size_t totalSent = 0;
    const char *ptr = (const char *)data;

    while (totalSent < size)
    {
        ssize_t sizeSent = send(sockid, ptr + totalSent, size - totalSent, 0);

        if (sizeSent == 0)
            return;
        if (sizeSent < 0)
        {
            std::cout << "send error: " << strerror(errno) << std::endl;
            throw std::runtime_error("send failed");
        }
        totalSent += sizeSent;
    }
}

void recvAll(int sockid, void *data, size_t size)
{
    size_t totalReceived = 0;
    char *ptr = (char *)data;

    while (totalReceived < size)
    {
        ssize_t sizeReceived = recv(sockid, ptr + totalReceived, size - totalReceived, 0);

        if (sizeReceived < 0)
            throw std::runtime_error("recv failed");

        totalReceived += sizeReceived;
    }
}

void printBytes(const char* data, size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << (static_cast<unsigned int>(static_cast<unsigned char>(data[i])))
                  << " ";
    }
    std::cout << std::dec << std::endl;
}