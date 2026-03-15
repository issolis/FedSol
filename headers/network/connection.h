#ifndef CONNECTION_H
#define CONNECTION_H
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "serializer/serializer.h"
#include "network/net_utils.h"


class Connection
{
private:
    /* data */
public:
    Connection();
    static int createConnection(const std::string& serverIP, uint16_t port); 
    static void sendMessage(int sockID, int code, std::string &content, std::string &password, bool closeConnection); 
};




#endif