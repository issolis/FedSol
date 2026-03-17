#ifndef SERIALIZER_H
#define SERIALIZER_H
#include <vector>
#include <utility>
#include <iostream>
#include "network/message.h"
#include <cstring>
#include <arpa/inet.h>
#include <models/Architecture.h>


class Serializer
{
private:
public:
    Serializer();
    static std::vector<char> serializeMessage(int code, const std::string &password, const std::string &content);
    static Message deserializeMessage(const std::vector<char>& buffer); 
    static std::vector<char> serializeID(uint32_t id);
    static uint32_t deserializeID(const std::vector<char> &buffer);
    static std::vector<char> serializeArchitecture(const Architecture& arch);
    static Architecture deserializeArchitecture(const std::vector<char>& buffer); 
};

#endif