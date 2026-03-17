#include "models/Model.h"

Model::Model(uint32_t id, Architecture& architecture, std::vector<float>& weights)
{
    this->id = id;
    this->architecture = architecture;
    this->weights = weights;
}

Model::Model()
{
}

std::vector<uint32_t> Model::getNonSerializedPos()
{
    return nonSerializedPos;
}

Architecture Model::getArchitecture() const
{
    return architecture;
}

std::vector<float> Model::getWeights() const
{
    return weights;
}

std::vector<char> Model::getSerializedPos()
{
    return serializedPos;
}

uint32_t Model::getID() const
{
    return id;
}

std::vector<char> Model::getSerializedModel()
{
    return serialedModel;
}