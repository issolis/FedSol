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

    bool equals(const Architecture& other) const; 

    bool verifyArchitecture(const Architecture &arch_1, const Architecture &arch_2); 


    void printArchitecture() const;
    const std::vector<Layer>& getLayers() const;
};

#endif