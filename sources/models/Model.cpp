#include "models/Model.h"

Model::Model(uint32_t id, Architecture architecture, std::vector<uint32_t> weights)
{
    this->id = id;
    this->architecture = architecture;
    this->weights = weights;

    this->serializeModel(); 
}

Model::  Model(std::vector<char> serializedPos, std::vector<char> serializedModel){
    this->serializedPos = serializedPos;
    Model model = deserializeModel(serializedModel);
    
    this->id = model.id; 
    this->architecture =  model.architecture; 
    this->weights = model.weights; 
    this->nonSerializedPos = model.nonSerializedPos; 
    this->serializedPos = model.serializedPos; 

    this->serializeModel();
}

std::vector<char> Model::serializeModel(){
    nonSerializedPos.clear();
    uint32_t numLayers = architecture.getLayers().size();
    uint32_t numWeights = weights.size();

    uint32_t totalSize =
        sizeof(id) +
        sizeof(numLayers) +
        numLayers * sizeof(Layer) +
        sizeof(numWeights) +
        numWeights * sizeof(uint32_t);

    std::vector<char> buffer(totalSize);
    char *ptr = buffer.data();

    memcpy(ptr, &id, sizeof(id));
    ptr += sizeof(id);
    nonSerializedPos.push_back(sizeof(id));

    memcpy(ptr, &numLayers, sizeof(numLayers));
    ptr += sizeof(numLayers);
    nonSerializedPos.push_back(sizeof(numLayers));  

    memcpy(ptr, architecture.getLayers().data(), numLayers * sizeof(Layer));
    ptr += numLayers * sizeof(Layer);
    nonSerializedPos.push_back(numLayers * sizeof(Layer));

    memcpy(ptr, &numWeights, sizeof(numWeights));
    ptr += sizeof(numWeights);
    nonSerializedPos.push_back(sizeof(numWeights));

    memcpy(ptr, weights.data(), numWeights * sizeof(uint32_t));

    serializedPos = serializePos(); 
    serialedModel = buffer; 

    return buffer;
}

 std::vector<char> Model::serializePos(){
    
    uint32_t totalSize = sizeof(uint32_t) * nonSerializedPos.size(); 
    std::vector<char> buffer(totalSize);
    char *ptr = buffer.data();

    for (size_t i = 0; i < nonSerializedPos.size(); i++)
    {
        uint32_t pos = nonSerializedPos[i];
        memcpy(ptr, &pos, sizeof(pos));
        ptr += sizeof(pos);
    }
    return buffer;
}

std::vector<uint32_t> Model::deserializePos(const std::vector<char>& buffer){
    nonSerializedPos.clear();

    const char* ptr = buffer.data();

    size_t count = buffer.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; i++)
    {
        uint32_t value;

        memcpy(&value, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        nonSerializedPos.push_back(value);
    }

    return nonSerializedPos; 
}

Model Model::deserializeModel(const std::vector<char> &buffer){
    deserializePos(serializedPos);

    const char* ptr = buffer.data();

    uint32_t id;
    memcpy(&id, ptr, sizeof(id));
    ptr += nonSerializedPos[0];

    Architecture architecture;
    uint32_t numLayers;
    memcpy(&numLayers, ptr, sizeof(numLayers));
    ptr += nonSerializedPos[1];

    for (size_t i = 0; i < numLayers; i++)
    {
        Layer layer;
        memcpy(&layer, ptr, sizeof(Layer));
        ptr += nonSerializedPos[2] / numLayers;
        architecture.addLayer(layer);
    }

    uint32_t numWeights;
    memcpy(&numWeights, ptr, sizeof(numWeights));
    ptr += nonSerializedPos[3];

    std::vector<uint32_t> weights(numWeights);
    for (size_t i = 0; i < numWeights; i++)
    {
        uint32_t weight;
        memcpy(&weight, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        weights[i] = weight;
    }

    
    Model model(id, architecture, weights);
    model.nonSerializedPos = nonSerializedPos;
    return model;
}

std::vector<uint32_t> Model::getNonSerializedPos() {
    return nonSerializedPos;
}

Architecture Model::getArchitecture() const
{
    return architecture;
}

std::vector<uint32_t> Model::getWeights() const
{
    return weights;
}

std::vector<char> Model::getSerializedPos()
{
    return serializedPos;
}

uint32_t Model::getID(){
    return id; 
}

std::vector<char> Model::getSerializedModel(){
    return serialedModel; 
}