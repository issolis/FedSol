#ifndef JSONMANAGER_H
#define JSONMANAGER_H

#include <fstream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

class JSONManager
{
private:
public:
    static nlohmann::json load(const std::string &path);
    static void updateWeightsInJSON(
        const std::string &path,
        const std::vector<float> &weights);
};

#endif