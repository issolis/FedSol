#include "serializer/serializer.h"


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

std::vector<char> Serializer::serializeArchitecture(const Architecture& arch)
{
    const std::vector<Layer>& layers = arch.getLayers();
    uint32_t numLayers = layers.size();

    size_t layerSize =
        sizeof(uint32_t) +       // type
        3*sizeof(uint32_t) +     // input_dim
        3*sizeof(uint32_t) +     // output_dim
        sizeof(uint32_t) +       // kernel
        sizeof(uint32_t) +       // stride
        sizeof(uint32_t) +       // padding
        sizeof(uint32_t) +       // in_features
        sizeof(uint32_t) +       // out_features
        sizeof(uint32_t);        // activation

    size_t totalSize = sizeof(uint32_t) + numLayers * layerSize;

    std::vector<char> buffer(totalSize);
    char* ptr = buffer.data();

    uint32_t net_numLayers = htonl(numLayers);
    memcpy(ptr, &net_numLayers, sizeof(net_numLayers));
    ptr += sizeof(net_numLayers);

    for (const Layer& layer : layers)
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

Architecture Serializer::deserializeArchitecture(const std::vector<char>& buffer)
{
    Architecture arch;

    const char* ptr = buffer.data();

    uint32_t numLayers;
    memcpy(&numLayers, ptr, sizeof(numLayers));
    ptr += sizeof(numLayers);

    numLayers = ntohl(numLayers);

    for(uint32_t i = 0; i < numLayers; i++)
    {
        Layer layer;

        uint32_t type;
        memcpy(&type, ptr, sizeof(type));
        ptr += sizeof(type);
        layer.type = static_cast<LayerType>(ntohl(type));

        for(int j = 0; j < 3; j++)
        {
            uint32_t v;
            memcpy(&v, ptr, sizeof(v));
            ptr += sizeof(v);
            layer.input_dim[j] = ntohl(v);
        }

        for(int j = 0; j < 3; j++)
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