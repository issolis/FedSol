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

Architecture Model::getArchitecture() const
{
    return architecture;
}

std::vector<float> Model::getWeights() const
{
    return weights;
}

uint32_t Model::getID() const
{
    return id;
}

void Model::setWeights(std::vector<float>& weights){
    this->weights = weights;
}
