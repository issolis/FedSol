#ifndef DATASETUTILS_H
#define DATASETUTILS_H

#include <string>
#include <cstdint>

class DatasetUtils
{
public:
    static uint32_t getTotalSamplesFromNPZ(const std::string& datasetPath,
                                           const std::string& scriptPath);
};

#endif