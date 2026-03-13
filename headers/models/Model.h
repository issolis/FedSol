#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include "Architecture.h"

class Model
{
private:
    uint32_t id;
    Architecture architecture;
    std::vector<uint32_t> weights;
    std::vector<uint32_t> nonSerializedPos;
    std::vector<char> serializedPos;
    std::vector<char> serialedModel;

    std::vector<uint32_t> deserializePos(const std::vector<char> &buffer);
    Model deserializeModel(const std::vector<char> &buffer);
    std::vector<char> serializePos();
    std::vector<char> serializeModel();

public:
    Model(
        uint32_t id,
        Architecture architecture,
        std::vector<uint32_t> weights);

    Model(std::vector<char> serializedPos, std::vector<char> serializedModel);

    std::vector<uint32_t> getNonSerializedPos();
    Architecture getArchitecture() const;
    std::vector<uint32_t> getWeights() const;
    std::vector<char> getSerializedPos();
    std::vector<char> getSerializedModel();

    uint32_t getID();
};

#endif