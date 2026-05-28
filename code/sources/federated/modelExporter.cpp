#include "federated/modelExporter.h"
#include <cstdlib>
#include "logger/logger.h"

bool ModelExporter::exportModel(const std::string& configPath,
                                const std::string& scriptPath)
{
    std::string command =
        "PYTHONPATH=code python3 -m FedSolPython.scripts." + scriptPath +
        " \"" + configPath + "\"";

    Logger::log(LogLevel::INFO, "[EXPORTER] Running: " + command);

    int result = system(command.c_str());
    if (result != 0)
    {
        Logger::log(LogLevel::ERROR, "[EXPORTER] Failed to export model");
        return false;
    }

    Logger::log(LogLevel::INFO, "[EXPORTER] Model exported successfully");
    return true;
}