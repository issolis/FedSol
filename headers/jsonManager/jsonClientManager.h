#ifndef JSONCLIENTMANAGER_H
#define JSONCLIENTMANAGER_H

#include <fstream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <jsonManager/jsonManager.h>
#include <network/client.h>

class JSONClientManager
{
private:
public:
    static void startClientFromJSON(const std::string& path);
};

#endif