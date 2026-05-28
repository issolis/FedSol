#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include "jsonManager/jsonClientManager.h"

using json = nlohmann::json;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_path>\n";
        return 1;
    }

    std::string path = argv[1];
    JSONClientManager::startClientFromJSON(path);

    return 0;
}