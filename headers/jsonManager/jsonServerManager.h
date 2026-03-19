#ifndef JSONSERVERMANAGER_H
#define JSONSERVERMANAGER_H

#include <fstream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <jsonManager/jsonManager.h>
#include <network/server.h>

class JSONServerManager
{
private:
public:
    static void startServerFromJSON(const std::string& path);
};

#endif