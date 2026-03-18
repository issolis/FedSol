#include "network/net_utils.h"
#include <iostream>
#include <cerrno>
#include <iomanip>
#include <cstring>
void sendAll(int sockid, const void *data, size_t size)
{
    size_t totalSent = 0;
    const char *ptr = static_cast<const char *>(data);

    while (totalSent < size)
    {
        ssize_t sizeSent = send(sockid,
                                ptr + totalSent,
                                size - totalSent,
                                MSG_NOSIGNAL);  

        if (sizeSent == 0)
        {
            throw std::runtime_error("connection closed");
        }

        if (sizeSent < 0)
        {
            if (errno == EINTR)
                continue; 

            if (errno == EPIPE || errno == ECONNRESET)
            {
                throw std::runtime_error("client disconnected");
            }

            std::cerr << "send error: " << strerror(errno) << std::endl;
            throw std::runtime_error("send failed");
        }

        totalSent += sizeSent;
    }
}

void recvAll(int sock, void* buffer, size_t length)
{
    size_t total = 0;
    char* ptr = static_cast<char*>(buffer);

    while (total < length)
    {
        ssize_t received = recv(sock, ptr + total, length - total, 0);

        if (received == 0)
        {
            throw std::runtime_error("Client disconnected (recv = 0)");
        }

        if (received < 0)
        {
            throw std::runtime_error("recv failed");
        }

        total += received;
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