#include "serializer/serializer.h"
#include <cstring>
#include <arpa/inet.h>

Serializer::Serializer()
{
}
std::vector<char> Serializer::serializeMessage(
    int code,
    const std::string& password,
    const std::string& content)
{
    uint32_t passwordSize = password.size();
    uint32_t contentSize = content.size();

    uint32_t totalSize =
        sizeof(code) +
        sizeof(passwordSize) + passwordSize +
        sizeof(contentSize) + contentSize;

    std::vector<char> buffer(totalSize);

    char* ptr = buffer.data();

    memcpy(ptr, &code, sizeof(code));
    ptr += sizeof(code);

    memcpy(ptr, &passwordSize, sizeof(passwordSize));
    ptr += sizeof(passwordSize);

    memcpy(ptr, password.data(), passwordSize);
    ptr += passwordSize;

    memcpy(ptr, &contentSize, sizeof(contentSize));
    ptr += sizeof(contentSize);

    memcpy(ptr, content.data(), contentSize);

    return buffer;
}

Message Serializer::deserializeMessage(const std::vector<char>& buffer)
{
    Message msg;
    const char* ptr = buffer.data();

    uint32_t passwordSize;
    uint32_t contentSize;

    memcpy(&msg.code, ptr, sizeof(msg.code));
    ptr += sizeof(msg.code);

    memcpy(&passwordSize, ptr, sizeof(passwordSize));
    ptr += sizeof(passwordSize);

    msg.password.assign(ptr, passwordSize);
    ptr += passwordSize;

    memcpy(&contentSize, ptr, sizeof(contentSize));
    ptr += sizeof(contentSize);

    msg.content.assign(ptr, contentSize);

    return msg;
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