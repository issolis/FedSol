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
    std::vector<char> serializePos();

public:
    Model(
        uint32_t id,
        Architecture architecture,
        std::vector<uint32_t> weights);

    Model();

    std::vector<char> serialize();
    std::vector<uint32_t> getNonSerializedPos() ;
    void deserializePos(const std::vector<char> &buffer);
    Model deserialize(const std::vector<char> &buffer);
    void setSerializedPos(const std::vector<char> &serializedPos);
    Architecture getArchitecture() const; 
    std::vector<uint32_t> getWeights() const; 
    std::vector<char> getSerializedPos();

    uint32_t getID(); 

};

#endif