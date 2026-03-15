#ifndef SERIALIZER_H
#define SERIALIZAER_H
#include <vector>
#include <utility>
#include <arpa/inet.h>
#include <iostream>

class Serializer
{
private:
public:
    Serializer();
    static std::vector<char> serializeMessage(uint32_t code, std::string content);
    static std::pair<uint32_t, std::string> deserializeMessage(std::vector<char> buffer);
    static std::vector<char> serializeID(uint32_t id);
    static uint32_t deserializeID(const std::vector<char> &buffer);
};

#endif