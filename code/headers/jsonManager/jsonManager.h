#ifndef JSONMANAGER_H
#define JSONMANAGER_H

#include <fstream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <mutex>

class JSONManager
{
private:
    static std::mutex fileMutex;
public:

    static std::vector<float> getWeightsFromJSON(const std::string &path);
    static nlohmann::json load(const std::string &path);
    static void updateWeightsInJSON(
        const std::string &path,
        const std::vector<float> &weights);
};

#endif