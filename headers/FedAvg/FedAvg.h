#ifndef FEDAVG_H
#define FEDAVG_H 

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <immintrin.h>

class FedAvg
{
private:
    
public:
    static std::vector<float> fedAvg(std::vector<std::vector<float>>& weights, std::vector<uint32_t> sizes); 
 
};

#endif