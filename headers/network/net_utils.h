#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <sys/socket.h>
#include <stdexcept>

void sendAll(int sockid, const void *data, size_t size);
void recvAll(int sockid, void *data, size_t size);
void printBytes(const char* data, size_t size); 

#endif