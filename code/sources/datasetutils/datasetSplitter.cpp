#include "datasetutils/datasetSplitter.h"
#include "logger/logger.h"
#include <stdexcept>

void DatasetSplitter::split(const std::string& npzPath, int nClients)
{
    std::string cmd = "PYTHONPATH=code python3 -m FedSolPython.scripts.split_dataset "
                      + npzPath + " " + std::to_string(nClients);

    Logger::log(LogLevel::INFO, "[DatasetSplitter] Running: " + cmd);

    int result = system(cmd.c_str());

    if (result != 0)
    {
        Logger::log(LogLevel::ERROR, "[DatasetSplitter] split_dataset failed with code: " + std::to_string(result));
        throw std::runtime_error("DatasetSplitter failed with code: " + std::to_string(result));
    }

    Logger::log(LogLevel::INFO, "[DatasetSplitter] Split completed successfully");
}