#ifndef ARCHITECTURE_H
#define ARCHITECTURE_H

#include <vector>
#include "models/Layer.h"
#include <iostream>

class Architecture
{
private:
    std::vector<Layer> layers;
public:
    Architecture();
    ~Architecture();
    void addLayer(const Layer& layer);
    void printArchitecture();
    std::vector<Layer> getLayers() const;
   
};







#endif