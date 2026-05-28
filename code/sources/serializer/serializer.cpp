#include "serializer/serializer.h"

Serializer::Serializer()
{
}

std::vector<char> Serializer::serializeAuthMessage(int code, const std::string& password, const std::string& content)
{
    uint32_t passSize = password.size();
    uint32_t contentSize = content.size();

    uint32_t totalSize =
        sizeof(code) +
        sizeof(passSize) + passSize +
        sizeof(contentSize) + contentSize;

    std::vector<char> buffer(totalSize);
    char* ptr = buffer.data();

    memcpy(ptr, &code, sizeof(code));
    ptr += sizeof(code);

    memcpy(ptr, &passSize, sizeof(passSize));
    ptr += sizeof(passSize);

    memcpy(ptr, password.data(), passSize);
    ptr += passSize;

    memcpy(ptr, &contentSize, sizeof(contentSize));
    ptr += sizeof(contentSize);

    memcpy(ptr, content.data(), contentSize);

    return buffer;
}

AuthMessage Serializer::deserializeAuthMessage(const std::vector<char>& buffer)
{
    AuthMessage msg;
    const char* ptr = buffer.data();

    memcpy(&msg.code, ptr, sizeof(msg.code));
    ptr += sizeof(msg.code);

    uint32_t passSize;
    memcpy(&passSize, ptr, sizeof(passSize));
    ptr += sizeof(passSize);

    msg.password = std::string(ptr, passSize);
    ptr += passSize;

    uint32_t contentSize;
    memcpy(&contentSize, ptr, sizeof(contentSize));
    ptr += sizeof(contentSize);

    msg.content = std::string(ptr, contentSize);

    return msg;
}

std::vector<char> Serializer::serializeMessage(int code, const std::string& content)
{
    uint32_t contentSize = content.size();

    uint32_t totalSize =
        sizeof(code) +
        sizeof(contentSize) + contentSize;

    std::vector<char> buffer(totalSize);
    char* ptr = buffer.data();

    memcpy(ptr, &code, sizeof(code));
    ptr += sizeof(code);

    memcpy(ptr, &contentSize, sizeof(contentSize));
    ptr += sizeof(contentSize);

    memcpy(ptr, content.data(), contentSize);

    return buffer;
}

Message Serializer::deserializeMessage(const std::vector<char>& buffer)
{
    Message msg;
    const char* ptr = buffer.data();

    memcpy(&msg.code, ptr, sizeof(msg.code));
    ptr += sizeof(msg.code);

    uint32_t contentSize;
    memcpy(&contentSize, ptr, sizeof(contentSize));
    ptr += sizeof(contentSize);

    msg.content = std::string(ptr, contentSize);

    return msg;
}

uint32_t Serializer::deserializeID(const std::vector<char> &buffer)
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

std::vector<char> Serializer::serializeArchitecture(const Architecture &arch)
{
    const std::vector<Layer> &layers = arch.getLayers();
    uint32_t numLayers = layers.size();

    size_t layerSize =
        sizeof(uint32_t) +     // type
        3 * sizeof(uint32_t) + // input_dim
        3 * sizeof(uint32_t) + // output_dim
        sizeof(uint32_t) +     // kernel
        sizeof(uint32_t) +     // stride
        sizeof(uint32_t) +     // padding
        sizeof(uint32_t) +     // in_features
        sizeof(uint32_t) +     // out_features
        sizeof(uint32_t);      // activation

    size_t totalSize = sizeof(uint32_t) + numLayers * layerSize;

    std::vector<char> buffer(totalSize);
    char *ptr = buffer.data();

    uint32_t net_numLayers = htonl(numLayers);
    memcpy(ptr, &net_numLayers, sizeof(net_numLayers));
    ptr += sizeof(net_numLayers);

    for (const Layer &layer : layers)
    {
        uint32_t net_type = htonl(static_cast<uint32_t>(layer.type));
        memcpy(ptr, &net_type, sizeof(net_type));
        ptr += sizeof(net_type);

        for (int i = 0; i < 3; i++)
        {
            uint32_t v = htonl(layer.input_dim[i]);
            memcpy(ptr, &v, sizeof(v));
            ptr += sizeof(v);
        }

        for (int i = 0; i < 3; i++)
        {
            uint32_t v = htonl(layer.output_dim[i]);
            memcpy(ptr, &v, sizeof(v));
            ptr += sizeof(v);
        }

        uint32_t kernel = htonl(layer.kernel_size);
        memcpy(ptr, &kernel, sizeof(kernel));
        ptr += sizeof(kernel);

        uint32_t stride = htonl(layer.stride);
        memcpy(ptr, &stride, sizeof(stride));
        ptr += sizeof(stride);

        uint32_t padding = htonl(layer.padding);
        memcpy(ptr, &padding, sizeof(padding));
        ptr += sizeof(padding);

        uint32_t in_f = htonl(layer.in_features);
        memcpy(ptr, &in_f, sizeof(in_f));
        ptr += sizeof(in_f);

        uint32_t out_f = htonl(layer.out_features);
        memcpy(ptr, &out_f, sizeof(out_f));
        ptr += sizeof(out_f);

        uint32_t act = htonl(static_cast<uint32_t>(layer.activation));
        memcpy(ptr, &act, sizeof(act));
        ptr += sizeof(act);
    }

    return buffer;
}

Architecture Serializer::deserializeArchitecture(const std::vector<char> &buffer)
{
    Architecture arch;

    const char *ptr = buffer.data();

    uint32_t numLayers;
    memcpy(&numLayers, ptr, sizeof(numLayers));
    ptr += sizeof(numLayers);

    numLayers = ntohl(numLayers);

    for (uint32_t i = 0; i < numLayers; i++)
    {
        Layer layer;

        uint32_t type;
        memcpy(&type, ptr, sizeof(type));
        ptr += sizeof(type);
        layer.type = static_cast<LayerType>(ntohl(type));

        for (int j = 0; j < 3; j++)
        {
            uint32_t v;
            memcpy(&v, ptr, sizeof(v));
            ptr += sizeof(v);
            layer.input_dim[j] = ntohl(v);
        }

        for (int j = 0; j < 3; j++)
        {
            uint32_t v;
            memcpy(&v, ptr, sizeof(v));
            ptr += sizeof(v);
            layer.output_dim[j] = ntohl(v);
        }

        uint32_t kernel;
        memcpy(&kernel, ptr, sizeof(kernel));
        ptr += sizeof(kernel);
        layer.kernel_size = ntohl(kernel);

        uint32_t stride;
        memcpy(&stride, ptr, sizeof(stride));
        ptr += sizeof(stride);
        layer.stride = ntohl(stride);

        uint32_t padding;
        memcpy(&padding, ptr, sizeof(padding));
        ptr += sizeof(padding);
        layer.padding = ntohl(padding);

        uint32_t in_f;
        memcpy(&in_f, ptr, sizeof(in_f));
        ptr += sizeof(in_f);
        layer.in_features = ntohl(in_f);

        uint32_t out_f;
        memcpy(&out_f, ptr, sizeof(out_f));
        ptr += sizeof(out_f);
        layer.out_features = ntohl(out_f);

        uint32_t act;
        memcpy(&act, ptr, sizeof(act));
        ptr += sizeof(act);
        layer.activation = static_cast<ActivationType>(ntohl(act));

        arch.addLayer(layer);
    }

    return arch;
}

std::vector<char> Serializer::serializeWeights(const std::vector<float> &weights)
{
    uint32_t numWeights = weights.size();

    size_t totalSize =
        sizeof(uint32_t) +
        numWeights * sizeof(uint32_t);

    std::vector<char> buffer(totalSize);
    char *ptr = buffer.data();

    uint32_t net_num = htonl(numWeights);
    memcpy(ptr, &net_num, sizeof(net_num));
    ptr += sizeof(net_num);

    for (float w : weights)
    {
        uint32_t raw;
        memcpy(&raw, &w, sizeof(float)); // reinterpret float → uint32_t

        raw = htonl(raw);

        memcpy(ptr, &raw, sizeof(raw));
        ptr += sizeof(raw);
    }

    return buffer;
}

std::vector<float> Serializer::deserializeWeights(const std::vector<char>& buffer)
{
    const char* ptr = buffer.data();

    uint32_t numWeights;
    memcpy(&numWeights, ptr, sizeof(numWeights));
    ptr += sizeof(numWeights);

    numWeights = ntohl(numWeights);

    std::vector<float> weights(numWeights);

    for(uint32_t i = 0; i < numWeights; i++)
    {
        uint32_t raw;
        memcpy(&raw, ptr, sizeof(raw));
        ptr += sizeof(raw);

        raw = ntohl(raw);

        float w;
        memcpy(&w, &raw, sizeof(float));  // reinterpret back

        weights[i] = w;
    }

    return weights;
}