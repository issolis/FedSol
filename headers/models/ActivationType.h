#ifndef ACTIVATIONTYPE_H
#define ACTIVATIONTYPE_H

#include <cstdint>

enum class ActivationType : uint32_t
{
    NONEACT = 0,
    ACT_NONE = 1,
    ACT_RELU = 2,
    ACT_SIGMOID = 3,
    ACT_TANH = 4,
    ACT_SOFTMAX = 5
};

#endif