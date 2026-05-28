#include "jsonManager/jsonManager.h"

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

std::mutex JSONManager::fileMutex;

json JSONManager::load(const std::string& path)
{
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ifstream file(path);

    if (!file.is_open())
        throw std::runtime_error("Could not open JSON file");

    json j;
    file >> j;

    return j;
}

void JSONManager::updateWeightsInJSON(
    const std::string& path,
    const std::vector<float>& weights)
{
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ifstream input(path);

    if (!input.is_open())
        throw std::runtime_error("Could not open JSON file");

    json j;
    input >> j;
    input.close();

    j["model"]["weights"] = weights;

    std::ofstream output(path);

    if (!output.is_open())
        throw std::runtime_error("Could not open JSON file for writing");

    output << j.dump(4);
}

std::vector<float> JSONManager::getWeightsFromJSON(
    const std::string& path)
{
    json j = JSONManager::load(path);

    if (!j.contains("model") || !j["model"].contains("weights"))
    {
        throw std::runtime_error(
            "JSON does not contain model.weights");
    }

    return j["model"]["weights"].get<std::vector<float>>();
}