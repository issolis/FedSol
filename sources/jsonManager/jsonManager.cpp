#include "jsonManager/jsonManager.h"

using json = nlohmann::json;

json JSONManager::load(const std::string& path)
{
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
    json j = JSONManager::load(path);

    j["model"]["weights"] = weights;

    std::ofstream file(path);
    file << j.dump(4);
}