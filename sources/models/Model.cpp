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


uint32_t Model::getID() const
{
    return id;
}

void Model::setWeights(const std::vector<float>& weights)
{
    std::lock_guard<std::mutex> lock(weightsMutex);
    this->weights = weights;
}

std::vector<float> Model::getWeights() const
{
    std::lock_guard<std::mutex> lock(weightsMutex);
    return weights;
}