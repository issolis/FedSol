#include "security/envUtils.h"
#include <fstream>

std::string loadEnvValue(const std::string& key)
{
    std::ifstream file(".env");
    std::string line;

    while(std::getline(file, line))
    {
        size_t pos = line.find('=');

        if(pos == std::string::npos)
            continue;

        std::string k = line.substr(0, pos);
        std::string v = line.substr(pos + 1);

        if(k == key)
            return v;
    }

    return "";
}