#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <string>
#include <sstream>
#include <iomanip>
#include "security/envUtils.h"
#include <iostream>
#include "security/SHA256.h"


class AuthManager
{
private:
    std::string storedHash;

public:
    AuthManager();
    bool authenticate(const std::string &password);
    std::string hashPassword(const std::string &password);
};

#endif