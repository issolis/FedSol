#include "serializer/serializer.h"
#include <cstring>

Serializer::Serializer()
{
}

std::vector<char> Serializer::serializeMessage(uint32_t code, std::string content)
{
    uint32_t contentSize = content.size();

    uint32_t totalSize =
        sizeof(uint32_t) + // code
        sizeof(uint32_t) + // content size
        contentSize;       // content

    std::vector<char> buffer(totalSize);
    char *ptr = buffer.data();

    memcpy(ptr, &code, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, &contentSize, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, content.data(), contentSize);

    return buffer;
}

std::pair<uint32_t, std::string> Serializer::deserializeMessage(std::vector<char> buffer)
{
    const char *ptr = buffer.data();

    uint32_t code;
    uint32_t contentSize;

    memcpy(&code, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(&contentSize, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    std::string content(ptr, ptr + contentSize);

    return {code, content};
}

uint32_t Serializer::deserializeID(const std::vector<char>& buffer)
{
    uint32_t net_id;

    memcpy(&net_id, buffer.data(), sizeof(uint32_t));

    return ntohl(net_id);
}

std::vector<char> Serializer::serializeID(uint32_t id)
{
    uint32_t net_id = htonl(id);

    std::vector<char> buffer(sizeof(uint32_t));

    memcpy(buffer.data(), &net_id, sizeof(uint32_t));

    return buffer;
}