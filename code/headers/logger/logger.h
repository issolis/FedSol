#ifndef LOGGER_H
#define LOGGER_H 

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG
};

class Logger {
public:
    static void init(const std::string& filename);

    static void log(LogLevel level, const std::string& msg);

    static void close();

private:
    static std::ofstream file;
    static std::mutex mtx;

    static std::string levelToString(LogLevel level);
};

#endif