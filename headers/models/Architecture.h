#ifndef ARCHITECTURE_H
#define ARCHITECTURE_H

#include <vector>
#include <iostream>
#include "models/Layer.h"

class Architecture
{
private:
    std::vector<Layer> layers;

public:
    Architecture();
    ~Architecture();

    void addLayer(const Layer& layer);

    void printArchitecture();
    const std::vector<Layer>& getLayers() const;
};

#endif