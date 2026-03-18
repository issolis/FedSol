#include "security/AuthManager.h"

AuthManager::AuthManager()
{
    storedHash = loadEnvValue("SERVER_PASSWORD_HASH");
    if (storedHash.empty())
    {
        throw std::runtime_error("SERVER_PASSWORD_HASH not found in .env");
    }
}

bool AuthManager::authenticate(const std::string &password)
{
    std::string receivedHash = hashPassword(password);
    return (receivedHash) == storedHash;
}

std::string AuthManager::hashPassword(const std::string &password)
{
    SHA256 sha;
    sha.update(password);
    auto digest = sha.digest();
    return SHA256::toString(digest);
}