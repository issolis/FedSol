#include "serializer/modelSerializer.h"
#include <arpa/inet.h>
#include <cstring>

std::pair<std::vector<char>, std::vector<char>>
ModelSerializer::serialize(Model& model)
{
    std::vector<uint32_t> nonSerializedPos;

    uint32_t id = model.getID();
    const Architecture& architecture = model.getArchitecture();
    const std::vector<uint32_t>& weights = model.getWeights();

    const std::vector<Layer>& layers = architecture.getLayers();

    uint32_t numLayers = layers.size();
    uint32_t numWeights = weights.size();

    uint32_t totalSize =
        sizeof(uint32_t) +              
        sizeof(uint32_t) +              
        numLayers * sizeof(Layer) +     
        sizeof(uint32_t) +              
        numWeights * sizeof(uint32_t);  

    std::vector<char> buffer(totalSize);
    char* ptr = buffer.data();

    uint32_t net_id = htonl(id);
    memcpy(ptr, &net_id, sizeof(net_id));
    ptr += sizeof(net_id);

    nonSerializedPos.push_back(sizeof(net_id));

    uint32_t net_layers = htonl(numLayers);
    memcpy(ptr, &net_layers, sizeof(net_layers));
    ptr += sizeof(net_layers);

    nonSerializedPos.push_back(sizeof(net_layers));

    
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

    nonSerializedPos.push_back(numLayers * sizeof(Layer));

    uint32_t net_weights = htonl(numWeights);
    memcpy(ptr, &net_weights, sizeof(net_weights));
    ptr += sizeof(net_weights);

    nonSerializedPos.push_back(sizeof(net_weights));

    for (uint32_t w : weights)
    {
        uint32_t net_w = htonl(w);
        memcpy(ptr, &net_w, sizeof(net_w));
        ptr += sizeof(net_w);
    }

    return {serializePos(nonSerializedPos), buffer};
}

std::vector<char>
ModelSerializer::serializePos(const std::vector<uint32_t>& nonSerializedPos)
{
    uint32_t totalSize = sizeof(uint32_t) * nonSerializedPos.size();

    std::vector<char> buffer(totalSize);
    char* ptr = buffer.data();

    for (uint32_t pos : nonSerializedPos)
    {
        uint32_t netPos = htonl(pos); 
        memcpy(ptr, &netPos, sizeof(pos));
        ptr += sizeof(netPos);
    }

    return buffer;
}

std::vector<uint32_t>
ModelSerializer::deserializePos(const std::vector<char>& buffer)
{
    std::vector<uint32_t> nonSerializedPos;

    const char* ptr = buffer.data();
    size_t count = buffer.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; i++)
    {
        uint32_t netValue;
        memcpy(&netValue, ptr, sizeof(netValue));
        ptr += sizeof(netValue);
        uint32_t value = ntohl(netValue); 
        nonSerializedPos.push_back(value);
    }

    return nonSerializedPos;
}

Model ModelSerializer::deserialize(
    const std::vector<char>& serializedPos,
    const std::vector<char>& serializedModel)
{
    const char* ptr = serializedModel.data();

    std::vector<uint32_t> nonSerializedPos = deserializePos(serializedPos);

    uint32_t net_id;
    memcpy(&net_id, ptr, sizeof(net_id));
    uint32_t id = ntohl(net_id);

    ptr += nonSerializedPos[0];

    uint32_t net_layers;
    memcpy(&net_layers, ptr, sizeof(net_layers));
    uint32_t numLayers = ntohl(net_layers);

    ptr += nonSerializedPos[1];

    Architecture architecture;

    for (size_t i = 0; i < numLayers; i++)
    {
        Layer layer;

        uint32_t v;

        memcpy(&v, ptr, sizeof(v));
        layer.type = static_cast<LayerType>(ntohl(v));
        ptr += sizeof(v);

        for (int j = 0; j < 3; j++)
        {
            memcpy(&v, ptr, sizeof(v));
            layer.input_dim[j] = ntohl(v);
            ptr += sizeof(v);
        }

        for (int j = 0; j < 3; j++)
        {
            memcpy(&v, ptr, sizeof(v));
            layer.output_dim[j] = ntohl(v);
            ptr += sizeof(v);
        }

        memcpy(&v, ptr, sizeof(v));
        layer.kernel_size = ntohl(v);
        ptr += sizeof(v);

        memcpy(&v, ptr, sizeof(v));
        layer.stride = ntohl(v);
        ptr += sizeof(v);

        memcpy(&v, ptr, sizeof(v));
        layer.padding = ntohl(v);
        ptr += sizeof(v);

        memcpy(&v, ptr, sizeof(v));
        layer.in_features = ntohl(v);
        ptr += sizeof(v);

        memcpy(&v, ptr, sizeof(v));
        layer.out_features = ntohl(v);
        ptr += sizeof(v);

        memcpy(&v, ptr, sizeof(v));
        layer.activation = static_cast<ActivationType>(ntohl(v));
        ptr += sizeof(v);

        architecture.addLayer(layer);
    }

    uint32_t net_weights;
    memcpy(&net_weights, ptr, sizeof(net_weights));
    uint32_t numWeights = ntohl(net_weights);

    ptr += nonSerializedPos[3];
    std::vector<uint32_t> weights(numWeights);

    for (size_t i = 0; i < numWeights; i++)
    {
        uint32_t net_w;

        memcpy(&net_w, ptr, sizeof(net_w));
        ptr += sizeof(net_w);

        weights[i] = ntohl(net_w);
    }

    return Model(id, architecture, weights);
}