#ifndef LAYERTYPE_H
#define LAYERTYPE_H

#include <cstdint>

enum class LayerType : uint32_t
{
    NONETYPE = 0,
    LAYER_INPUT = 1,
    LAYER_CONV2D = 2,
    LAYER_MAXPOOL2D = 3,
    LAYER_FLATTEN = 4,
    LAYER_DENSE = 5,
    LAYER_BATCHNORM = 6,
    LAYER_DROPOUT = 7,
    LAYER_ACTIVATION = 8
};

#endif