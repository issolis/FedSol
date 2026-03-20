#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include "Architecture.h"
#include  <mutex>
class Model
{
private:
    uint32_t id;
    Architecture architecture;
    std::vector<float> weights;
      mutable std::mutex weightsMutex;


public:
    Model(
        uint32_t id,
        Architecture& architecture,
        std::vector<float>& weights);

    Model(); 

    Architecture getArchitecture() const;
    std::vector<float> getWeights() const;
    uint32_t getID() const;
    void setWeights(const std::vector<float>& weights); 
};

#endif