#ifndef LAYER_H
#define LAYER_H

#include "models/ActivationType.h"
#include "models/LayerType.h"
#include <cstring>

class Layer
{
private:
    /* data */
public:
    Layer();

    LayerType type;

    int input_dim[3];
    int output_dim[3];

    int kernel_size;
    int stride;
    int padding;

    int in_features;
    int out_features;

    ActivationType activation;
};


#endif