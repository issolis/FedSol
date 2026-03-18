#include "logger/logger.h"
#include <ctime>
#include <cstring>

std::ofstream Logger::file;
std::mutex Logger::mtx;

void Logger::init(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mtx);

    if (file.is_open()) return; 

    file.open(filename, std::ios::out | std::ios::app);
}
std::string Logger::levelToString(LogLevel level) {
    switch(level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);

    if (!file.is_open()) return;

    std::time_t now = std::time(nullptr);
    char* dt = std::ctime(&now);

    if (dt) dt[strlen(dt) - 1] = '\0';

    file << "[" << dt << "] "
         << "[" << levelToString(level) << "] "
         << msg << std::endl;

    file.flush();
}

void Logger::close() {
    if (file.is_open()) {
        file.close();
    }
}