#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "Architecture.h"

class Model
{
private:
    uint32_t id;
    Architecture architecture;
    std::vector<float> weights;
    mutable std::mutex weightsMutex;

public:
    Model(uint32_t id, Architecture& architecture, std::vector<float>& weights);
    Model();

    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    Architecture getArchitecture() const;
    std::vector<float> getWeights() const;
    uint32_t getID() const;
    void setWeights(const std::vector<float>& weights);
};

#endif